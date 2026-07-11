#include "processor.h"

#include "config.h"
#include "const.h"

namespace Steinberg {
namespace Vst {

namespace {
constexpr int32 kNumInputEventChannels = 1;
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
  voices_ = std::array<Voice, kMaxVoices>{};
  nextVoiceAge_ = 0;
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
    switch (paramId) {
      case kParamPitchBend:
        pitchBend_ = 2 * (value - 0.5);
        break;
      case kParamVolume:
        volume_ = value;
        break;
      case kParamWaveform: {
        int8 waveformIndex = static_cast<int8>(
          value * (static_cast<int8>(Waveform::kNumWaveforms) - 1) + kEpsilon
        );
        for (Voice& voice : voices_) {
          voice.setWaveform(waveformIndex);
        }
        break;
      }
      case kParamDcoEgRate0:
      case kParamDcoEgRate1:
      case kParamDcoEgRate2:
      case kParamDcoEgRate3:
      case kParamDcoEgRate4:
      case kParamDcoEgRate5:
      case kParamDcoEgRate6:
      case kParamDcoEgRate7:
        for (Voice& voice : voices_) {
          voice.setEgRate(paramId, paramId - kParamDcoEgRate0, value);
        }
        break;
      case kParamDcoEgLevel0:
      case kParamDcoEgLevel1:
      case kParamDcoEgLevel2:
      case kParamDcoEgLevel3:
      case kParamDcoEgLevel4:
      case kParamDcoEgLevel5:
      case kParamDcoEgLevel6:
        for (Voice& voice : voices_) {
          voice.setEgLevel(paramId, paramId - kParamDcoEgLevel0, value);
        }
        break;
      case kParamDcwEgRate0:
      case kParamDcwEgRate1:
      case kParamDcwEgRate2:
      case kParamDcwEgRate3:
      case kParamDcwEgRate4:
      case kParamDcwEgRate5:
      case kParamDcwEgRate6:
      case kParamDcwEgRate7:
        for (Voice& voice : voices_) {
          voice.setEgRate(paramId, paramId - kParamDcwEgRate0, value);
        }
        break;
      case kParamDcwEgLevel0:
      case kParamDcwEgLevel1:
      case kParamDcwEgLevel2:
      case kParamDcwEgLevel3:
      case kParamDcwEgLevel4:
      case kParamDcwEgLevel5:
      case kParamDcwEgLevel6:
        for (Voice& voice : voices_) {
          voice.setEgLevel(paramId, paramId - kParamDcwEgLevel0, value);
        }
        break;
      case kParamDcaEgRate0:
      case kParamDcaEgRate1:
      case kParamDcaEgRate2:
      case kParamDcaEgRate3:
      case kParamDcaEgRate4:
      case kParamDcaEgRate5:
      case kParamDcaEgRate6:
      case kParamDcaEgRate7:
        for (Voice& voice : voices_) {
          voice.setEgRate(paramId, paramId - kParamDcaEgRate0, value);
        }
        break;
      case kParamDcaEgLevel0:
      case kParamDcaEgLevel1:
      case kParamDcaEgLevel2:
      case kParamDcaEgLevel3:
      case kParamDcaEgLevel4:
      case kParamDcaEgLevel5:
      case kParamDcaEgLevel6:
        for (Voice& voice : voices_) {
          voice.setEgLevel(paramId, paramId - kParamDcaEgLevel0, value);
        }
        break;
      case kParamDcoEgSustainPoint:
      case kParamDcwEgSustainPoint:
      case kParamDcaEgSustainPoint: {
        int8 point = static_cast<int8>(value * (kNumEgSustainPointOptions - 1) + kEpsilon);
        for (Voice& voice : voices_) {
          voice.setEgSustainPoint(paramId, point);
        }
        break;
      }
      case kParamDcoEgEndPoint:
      case kParamDcwEgEndPoint:
      case kParamDcaEgEndPoint: {
        int8 point = static_cast<int8>(value * (kNumEgEndPointOptions - 1) + kEpsilon);
        for (Voice& voice : voices_) {
          voice.setEgEndPoint(paramId, point);
        }
        break;
      }
      default:
        // do nothing
        break;
    }
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

Voice* PDProcessor::allocateVoice() {
  for (Voice& voice : voices_) {
    if (voice.isFree()) {
      return &voice;
    }
  }

  Voice* oldest = &voices_[0];
  for (Voice& voice : voices_) {
    if (voice.age() < oldest->age()) {
      oldest = &voice;
    }
  }
  return oldest;
}

void PDProcessor::onNoteOn(int channel, int note, float velocity) {
  Voice* voice = allocateVoice();
  voice->noteOn(channel, note, nextVoiceAge_++);
}

void PDProcessor::onNoteOff(int channel, int note, float velocity) {
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
