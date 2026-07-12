#include "processor.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include "config.h"
#include "const.h"

namespace Steinberg {
namespace Vst {

namespace {
constexpr int32 kNumInputEventChannels = 1;

// Decodes a normalized value of a discrete parameter with `numOptions`
// states into its option index.
int32 decodeOption(ParamValue value, int32 numOptions) {
  return static_cast<int32>(value * (numOptions - 1) + kEpsilon);
}

// Decodes a normalized value of a symmetric signed discrete parameter
// (-range .. +range) into its plain value.
int32 decodeSigned(ParamValue value, int32 range) {
  return decodeOption(value, 2 * range + 1) - range;
}
}  // namespace

FUnknown* PDProcessor::createInstance(void*) {
  return (IAudioProcessor*)new PDProcessor();
}

PDProcessor::PDProcessor() {
  setControllerClass(ControllerUID);
  PDProcessor::initializeParameter();
}

void PDProcessor::initializeParameter() {
  pitchBend_ = 0.0;
  volume_ = 0.5;
  lineSelect_ = LineSelect::kLine1;
  mono_ = false;
  detuneOctave_ = 0;
  detuneNote_ = 0;
  detuneFine_ = 0;
  voices_ = std::array<Voice, kMaxVoices>{};
  nextVoiceAge_ = 0;
  heldNotes_.clear();
}

tresult PLUGIN_API PDProcessor::initialize(FUnknown* context) {
  tresult result = AudioEffect::initialize(context);
  if (result == kResultTrue) {
    addEventInput(STR16("EventInput"), kNumInputEventChannels);  // input: kNumInputEventChannels event
    addAudioOutput(STR16("AudioOutput"), SpeakerArr::kStereo);   // output: stereo audio
  }
  return result;
}

tresult PLUGIN_API PDProcessor::setupProcessing(ProcessSetup& setup) {
  tresult result = AudioEffect::setupProcessing(setup);
  if (result == kResultOk) {
    for (Voice& voice : voices_) {
      voice.setSampleRate(processSetup.sampleRate);
    }
  }
  return result;
}

tresult PLUGIN_API PDProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                   SpeakerArrangement* outputs, int32 numOuts) {
  if (numOuts == 1 && outputs[0] == SpeakerArr::kStereo) {
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  }
  return kResultFalse;
}

tresult PLUGIN_API PDProcessor::process(ProcessData& data) {
  processParameter(data.inputParameterChanges);
  processEvent(data.inputEvents);
  processReplacing(data);
  return kResultTrue;
}

void PDProcessor::processParameter(IParameterChanges* changes) {
  if (changes == nullptr) {
    return;
  }

  int32 sampleOffset;
  ParamValue value;

  for (int32 i = 0; i < changes->getParameterCount(); i++) {
    IParamValueQueue* queue = changes->getParameterData(i);
    if (queue == nullptr) {
      continue;
    }
    if (queue->getPoint(queue->getPointCount() - 1, sampleOffset, value) == kResultFalse) {
      continue;
    }

    int32 paramId = queue->getParameterId();
    if (paramId == kParamPitchBend) {
      pitchBend_ = 2 * (value - 0.5);
    } else if (paramId == kParamVolume) {
      volume_ = value;
    } else if (paramId == kParamLineSelect) {
      lineSelect_ = static_cast<LineSelect>(
        decodeOption(value, static_cast<int32>(LineSelect::kNumLineSelects))
      );
      for (Voice& voice : voices_) {
        voice.setLineSelect(lineSelect_);
      }
    } else if (paramId == kParamMonoPoly) {
      bool mono = value >= 0.5;
      if (mono != mono_) {
        mono_ = mono;
        releaseAllVoices();
        heldNotes_.clear();
      }
    } else if (paramId == kParamDetuneOctave) {
      detuneOctave_ = decodeSigned(value, kDetuneOctaveRange);
      updateDetune();
    } else if (paramId == kParamDetuneNote) {
      detuneNote_ = decodeSigned(value, kDetuneNoteRange);
      updateDetune();
    } else if (paramId == kParamDetuneFine) {
      detuneFine_ = decodeSigned(value, kDetuneFineRange);
      updateDetune();
    } else if (kParamLine1Begin <= paramId && paramId < kNumParams) {
      int32 rel = paramId - kParamLine1Begin;
      int32 line = rel / kNumLineParams;
      int32 offset = rel % kNumLineParams;
      for (Voice& voice : voices_) {
        voice.setLineParam(line, offset, value);
      }
    }
  }
}

void PDProcessor::updateDetune() {
  double cents = 1200.0 * detuneOctave_ + 100.0 * detuneNote_ + kDetuneFineStepCents * detuneFine_;
  double ratio = pow(2.0, cents / 1200.0);
  for (Voice& voice : voices_) {
    voice.setDetuneRatio(ratio);
  }
}

void PDProcessor::processEvent(IEventList* events) {
  if (events == nullptr) {
    return;
  }

  int32 numEvents = events->getEventCount();
  Event event;

  for (int32 i = 0; i < numEvents; i++) {
    if (events->getEvent(i, event) == kResultFalse) {
      continue;
    }

    switch (event.type) {
      case Event::kNoteOnEvent:
        onNoteOn(event.noteOn.channel, event.noteOn.pitch, event.noteOn.velocity);
        break;
      case Event::kNoteOffEvent:
        onNoteOff(event.noteOff.channel, event.noteOff.pitch, event.noteOff.velocity);
        break;
      default:
        // do nothing
        break;
    }
  }
}

void PDProcessor::releaseAllVoices() {
  for (Voice& voice : voices_) {
    if (voice.isActive()) {
      voice.noteOff();
    }
  }
}

int32 PDProcessor::effectiveMaxVoices() const {
  bool dualLine = lineSelect_ == LineSelect::kLine1Plus1Detuned
               || lineSelect_ == LineSelect::kLine1Plus2Detuned;
  return dualLine ? kMaxVoices / 2 : kMaxVoices;
}

Voice* PDProcessor::allocateVoice() {
  int32 numVoices = effectiveMaxVoices();
  for (int32 i = 0; i < numVoices; i++) {
    if (voices_[i].isFree()) {
      return &voices_[i];
    }
  }

  Voice* oldest = &voices_[0];
  for (int32 i = 1; i < numVoices; i++) {
    if (voices_[i].age() < oldest->age()) {
      oldest = &voices_[i];
    }
  }
  return oldest;
}

void PDProcessor::onNoteOn(int channel, int note, float velocity) {
  if (mono_) {
    heldNotes_.push_back(HeldNote{channel, note});
    voices_[0].noteOn(channel, note, nextVoiceAge_++);  // last-note priority
    return;
  }
  allocateVoice()->noteOn(channel, note, nextVoiceAge_++);
}

void PDProcessor::onNoteOff(int channel, int note, float velocity) {
  if (mono_) {
    for (int32 i = static_cast<int32>(heldNotes_.size()) - 1; i >= 0; i--) {
      if (heldNotes_[i].channel == channel && heldNotes_[i].note == note) {
        heldNotes_.erase(heldNotes_.begin() + i);
      }
    }
    if (voices_[0].isHeld(channel, note)) {
      if (!heldNotes_.empty()) {
        // Return to the most recently pressed key that is still held.
        voices_[0].noteOn(heldNotes_.back().channel, heldNotes_.back().note, nextVoiceAge_++);
      } else {
        voices_[0].noteOff();
      }
    }
    return;
  }

  for (Voice& voice : voices_) {
    if (voice.isHeld(channel, note)) {
      voice.noteOff();
    }
  }
}

void PDProcessor::processReplacing(ProcessData& data) {
  // outputs \in [-1, 1]^(data.outputs[0].numChannels, data.numSamples)
  Sample32* outL = data.outputs[0].channelBuffers32[0];
  Sample32* outR = data.outputs[0].channelBuffers32[1];

  for (int32 i = 0; i < data.numSamples; i++) {
    Sample32 value = static_cast<Sample32>(generate());
    outL[i] = value;
    outR[i] = value;
  }
}

double PDProcessor::generate() {
  double mixed = 0.0;
  for (Voice& voice : voices_) {
    if (voice.isActive()) {
      mixed += voice.generate(pitchBend_);
    }
  }

  double out = volume_ * kVoiceMixGain * mixed;
  if (out > 1.0) {
    out = 1.0;
  } else if (out < -1.0) {
    out = -1.0;
  }
  return out;
}

}  // namespace Vst
}  // namespace Steinberg
