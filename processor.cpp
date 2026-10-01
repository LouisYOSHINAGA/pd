#include "processor.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

#include <utility>

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstmessage.h"

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
  scopeExchange_ = std::make_unique<DataExchangeHandler>(
    this, [](DataExchangeHandler::Config& config, const ProcessSetup&) {
      config.blockSize = sizeof(float) * kScopeFrameSize;
      config.numBlocks = 8;
      config.userContextID = kScopeExchangeId;
      return true;
    }
  );
  paramSyncExchange_ = std::make_unique<DataExchangeHandler>(
    this, [](DataExchangeHandler::Config& config, const ProcessSetup&) {
      config.blockSize = sizeof(ParamSyncBlock);
      config.numBlocks = 8;
      config.userContextID = kParamSyncExchangeId;
      return true;
    }
  );
  heldNotes_.reserve(128);  // no allocation on the audio thread in practice
  retriggerNotes_.reserve(128);
  PDProcessor::initializeParameter();
}

ParamValue PDProcessor::defaultParamValue(int32 paramId) {
  switch (paramId) {
    case kParamPitchBend:
    case kParamDetuneOctave:  // signed parameters center on 0.5
    case kParamDetuneNote:
    case kParamDetuneFine:
    case kParamOctaveRange:
    case kParamMasterTune:
      return 0.5;
    case kParamVolume:
      return 0.5;
    default:
      return 0.0;
  }
}

void PDProcessor::initializeParameter() {
  voices_ = std::array<Voice, kMaxVoices>{};
  fadeVoices_ = std::array<Voice, kNumFadeVoices>{};
  nextFadeVoice_ = 0;
  nextVoiceAge_ = 0;
  heldNotes_.clear();
  for (int32 paramId = 0; paramId < kNumParams; paramId++) {
    applyParameter(paramId, defaultParamValue(paramId));
  }
}

tresult PLUGIN_API PDProcessor::initialize(FUnknown* context) {
  tresult result = AudioEffect::initialize(context);
  if (result == kResultTrue) {
    addEventInput(STR16("EventInput"), kNumInputEventChannels);  // input: kNumInputEventChannels event
    addAudioOutput(STR16("AudioOutput"), SpeakerArr::kStereo);   // output: stereo audio
  }
  return result;
}

tresult PLUGIN_API PDProcessor::connect(IConnectionPoint* other) {
  tresult result = AudioEffect::connect(other);
  scopeExchange_->onConnect(other, getHostContext());
  paramSyncExchange_->onConnect(other, getHostContext());
  return result;
}

tresult PLUGIN_API PDProcessor::disconnect(IConnectionPoint* other) {
  scopeExchange_->onDisconnect(other);
  paramSyncExchange_->onDisconnect(other);
  return AudioEffect::disconnect(other);
}

tresult PLUGIN_API PDProcessor::setActive(TBool state) {
  if (state) {
    scopeExchange_->onActivate(processSetup);
    paramSyncExchange_->onActivate(processSetup);
  } else {
    scopeExchange_->onDeactivate();
    paramSyncExchange_->onDeactivate();
  }
  return AudioEffect::setActive(state);
}

tresult PLUGIN_API PDProcessor::setupProcessing(ProcessSetup& setup) {
  tresult result = AudioEffect::setupProcessing(setup);
  if (result == kResultOk) {
    internalTickStep_ = kInternalSampleRate / processSetup.sampleRate;
    resamplePhase_ = 1.0;
    prevTickSample_ = 0.0;
    currTickSample_ = 0.0;
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
  applyPendingPreset();
  processParameter(data.inputParameterChanges);
  sendParamSync();
  processEvent(data.inputEvents);
  processReplacing(data);
  return kResultTrue;
}

void PDProcessor::applyParameter(int32 paramId, ParamValue value) {
  if (paramId < 0 || kNumParams <= paramId) {
    return;
  }
  paramValues_[paramId] = value;

  if (paramId == kParamPitchBend) {
    pitchBend_ = 2 * (value - 0.5);
  } else if (paramId == kParamVolume) {
    volume_ = value;
  } else if (paramId == kParamLineSelect) {
    lineSelect_ = static_cast<LineSelect>(
      decodeOptionIndex(value, static_cast<int>(LineSelect::kNumLineSelects))
    );
  } else if (paramId == kParamMonoPoly) {
    bool mono = value >= 0.5;
    if (mono != mono_) {
      mono_ = mono;
      allNotesOff();
    }
  } else if (paramId == kParamDetuneOctave) {
    detuneOctave_ = decodeSignedOption(value, kDetuneOctaveRange);
    updateDetune();
  } else if (paramId == kParamDetuneNote) {
    detuneNote_ = decodeSignedOption(value, kDetuneNoteRange);
    updateDetune();
  } else if (paramId == kParamDetuneFine) {
    detuneFine_ = decodeSignedOption(value, kDetuneFineRange);
    updateDetune();
  } else if (paramId == kParamOctaveRange) {
    octaveRange_ = decodeSignedOption(value, kOctaveRangeMax);
  } else if (paramId == kParamLine1DcaKeyFollow || paramId == kParamLine2DcaKeyFollow) {
    dcaKeyFollow_[paramId - kParamLine1DcaKeyFollow] =
        static_cast<int8>(decodeOptionIndex(value, kNumKeyFollowOptions));
  } else if (paramId == kParamMasterTune) {
    masterTuneCents_ = decodeSignedOption(value, kMasterTuneRangeCents);
  } else if (paramId == kParamLine1DcwKeyFollow || paramId == kParamLine2DcwKeyFollow) {
    dcwKeyFollow_[paramId - kParamLine1DcwKeyFollow] =
        static_cast<int8>(decodeOptionIndex(value, kNumKeyFollowOptions));
  }
  // kParamCcEditLine only affects the controller's MIDI CC routing.

  // voices still releasing a preset switched away from keep their sound
  for (Voice& voice : voices_) {
    if (!voice.isFrozen()) {
      applyToVoice(voice, paramId);
    }
  }
  for (Voice& voice : fadeVoices_) {
    if (!voice.isFrozen()) {
      applyToVoice(voice, paramId);
    }
  }
}

void PDProcessor::applyToVoice(Voice& voice, int32 paramId) const {
  if (paramId == kParamLineSelect) {
    voice.setLineSelect(lineSelect_);
  } else if (paramId == kParamDetuneOctave || paramId == kParamDetuneNote
             || paramId == kParamDetuneFine) {
    voice.setDetuneRatio(detuneRatio_);
  } else if (kParamLine1Begin <= paramId && paramId < kParamCcEditLine) {
    int32 rel = paramId - kParamLine1Begin;
    voice.setLineParam(rel / kNumLineParams, rel % kNumLineParams, paramValues_[paramId]);
  } else if (paramId == kParamOctaveRange) {
    voice.setOctaveRange(octaveRange_);
  } else if (paramId == kParamLine1DcaKeyFollow || paramId == kParamLine2DcaKeyFollow) {
    int32 line = paramId - kParamLine1DcaKeyFollow;
    voice.setDcaKeyFollow(line, dcaKeyFollow_[line]);
  } else if (paramId == kParamMasterTune) {
    voice.setMasterTune(masterTuneCents_);
  } else if (paramId == kParamLine1DcwKeyFollow || paramId == kParamLine2DcwKeyFollow) {
    int32 line = paramId - kParamLine1DcwKeyFollow;
    voice.setDcwKeyFollow(line, dcwKeyFollow_[line]);
  }
}

void PDProcessor::syncVoice(Voice& voice) {
  for (int32 paramId = 0; paramId < kNumParams; paramId++) {
    applyToVoice(voice, paramId);
  }
  voice.setFrozen(false);
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
    // Only a real change is worth echoing: a value the controller already
    // holds (it originated there, or was echoed a moment ago) must not be
    // sent back, otherwise controller and processor ping-pong forever.
    bool changed = 0 <= paramId && paramId < kNumParams && paramValues_[paramId] != value;
    applyParameter(paramId, value);
    if (changed) {
      syncPending_[paramId] = true;
      syncValues_[paramId] = value;
      anySyncPending_ = true;
    }

    // The Mono/Poly triggers are momentary: pressing the same button twice
    // sends the same CC value twice, so forget the value right away to keep
    // the second press a change rather than a no-op.
    if (paramId == kParamMonoTrigger || paramId == kParamPolyTrigger) {
      paramValues_[paramId] = 0.0;
    }
  }
}

void PDProcessor::sendParamSync() {
  if (!anySyncPending_) {
    return;
  }
  DataExchangeBlock block = paramSyncExchange_->getCurrentOrNewBlock();
  if (block.blockID == InvalidDataExchangeBlockID) {
    return;  // queue full (or not connected): kept for the next call
  }
  ParamSyncBlock* sync = static_cast<ParamSyncBlock*>(block.data);
  sync->count = 0;
  for (int32 paramId = 0; paramId < kNumParams; paramId++) {
    if (syncPending_[paramId]) {
      sync->entries[sync->count++] = ParamSyncEntry{paramId, syncValues_[paramId]};
      syncPending_[paramId] = false;
    }
  }
  anySyncPending_ = false;
  paramSyncExchange_->sendCurrentBlock();
}

tresult PLUGIN_API PDProcessor::getState(IBStream* state) {
  if (state == nullptr) {
    return kResultFalse;
  }

  IBStreamer streamer(state, kLittleEndian);
  if (!streamer.writeInt32(kStateVersion)) {
    return kResultFalse;
  }
  // a preset not applied yet is already the state
  std::lock_guard<std::mutex> lock(pendingPresetMutex_);
  const ParamValue* values = hasPendingPreset_ ? pendingPreset_.data() : paramValues_.data();
  if (!streamer.writeDoubleArray(values, kNumParams)) {
    return kResultFalse;
  }
  return kResultTrue;
}

tresult PLUGIN_API PDProcessor::setState(IBStream* state) {
  if (state == nullptr) {
    return kResultFalse;
  }

  IBStreamer streamer(state, kLittleEndian);
  int32 version;
  if (!streamer.readInt32(version)) {
    return kResultFalse;
  }
  int32 numParams = numParamsOfStateVersion(version);
  if (numParams == 0) {
    return kResultFalse;
  }
  std::array<ParamValue, kNumParams> values;
  for (int32 paramId = 0; paramId < numParams; paramId++) {
    if (!streamer.readDouble(values[paramId])) {
      return kResultFalse;
    }
  }
  // parameters appended after the stream's version start from their defaults
  for (int32 paramId = numParams; paramId < kNumParams; paramId++) {
    values[paramId] = defaultParamValue(paramId);
  }
  // applied by process(): the host may call this while audio is running
  queuePreset(values);
  return kResultTrue;
}

tresult PLUGIN_API PDProcessor::notify(IMessage* message) {
  if (message != nullptr && strcmp(message->getMessageID(), kPresetMessageId) == 0) {
    const void* data = nullptr;
    uint32 size = 0;
    if (message->getAttributes()->getBinary(kPresetMessageDataAttr, data, size) == kResultTrue
        && size == sizeof(ParamValue) * kNumParams) {
      std::array<ParamValue, kNumParams> values;
      memcpy(values.data(), data, size);
      queuePreset(values);
    }
    return kResultTrue;
  }
  return AudioEffect::notify(message);
}

void PDProcessor::queuePreset(const std::array<ParamValue, kNumParams>& values) {
  std::lock_guard<std::mutex> lock(pendingPresetMutex_);
  pendingPreset_ = values;
  hasPendingPreset_ = true;
}

void PDProcessor::applyPendingPreset() {
  {
    // never wait on the audio thread: a preset being queued right now is
    // taken by the next call
    std::unique_lock<std::mutex> lock(pendingPresetMutex_, std::try_to_lock);
    if (!lock.owns_lock() || !hasPendingPreset_) {
      return;
    }
    presetValues_ = pendingPreset_;
    hasPendingPreset_ = false;
  }
  changePreset(presetValues_);
}

void PDProcessor::changePreset(const std::array<ParamValue, kNumParams>& values) {
  retriggerNotes_ = heldNotes_;
  heldNotes_.clear();
  for (Voice& voice : voices_) {
    if (voice.isActive()) {
      voice.setFrozen(true);
      voice.noteOff();
    }
  }
  for (Voice& voice : fadeVoices_) {
    if (voice.isActive()) {
      voice.setFrozen(true);  // fading out an old-preset sound
    }
  }
  // mono plays on voice 0: the old sound goes on releasing in a free voice
  if (mono_ && voices_[0].isActive()) {
    for (int32 i = 1; i < kMaxVoices; i++) {
      if (voices_[i].isFree()) {
        std::swap(voices_[0], voices_[i]);
        break;
      }
    }
  }

  for (int32 paramId = 0; paramId < kNumParams; paramId++) {
    applyParameter(paramId, values[paramId]);
  }
  for (const HeldNote& key : retriggerNotes_) {
    onNoteOn(key.channel, key.note, 1.f);
  }
}

void PDProcessor::updateDetune() {
  double cents = 1200.0 * detuneOctave_ + 100.0 * detuneNote_ + kDetuneFineStepCents * detuneFine_;
  detuneRatio_ = pow(2.0, cents / 1200.0);
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

void PDProcessor::allNotesOff() {
  releaseAllVoices();
  heldNotes_.clear();
}

int32 PDProcessor::effectiveMaxVoices() const {
  bool dualLine = lineSelect_ == LineSelect::kLine1Plus1Detuned
               || lineSelect_ == LineSelect::kLine1Plus2Detuned;
  return dualLine ? kMaxVoices / 2 : kMaxVoices;
}

Voice* PDProcessor::allocateVoice(int channel, int note) {
  int32 numVoices = effectiveMaxVoices();
  // the same key struck again takes back its own voice (held or releasing)
  for (int32 i = 0; i < numVoices; i++) {
    if (voices_[i].isPlaying(channel, note) && !voices_[i].isFrozen()) {
      return &voices_[i];
    }
  }
  for (int32 i = 0; i < numVoices; i++) {
    if (voices_[i].isFree()) {
      return &voices_[i];
    }
  }

  // steal the oldest released note; a held one only if every voice is held
  Voice* oldest = nullptr;
  for (bool releasedOnly : {true, false}) {
    for (int32 i = 0; i < numVoices; i++) {
      Voice& voice = voices_[i];
      if (releasedOnly && !voice.isReleasing()) {
        continue;
      }
      if (oldest == nullptr || voice.age() < oldest->age()) {
        oldest = &voice;
      }
    }
    if (oldest != nullptr) {
      break;
    }
  }
  return oldest;
}

void PDProcessor::onNoteOn(int channel, int note, float velocity) {
  heldNotes_.push_back(HeldNote{channel, note});
  // mono: last-note priority on voice 0
  startVoice(mono_ ? voices_[0] : *allocateVoice(channel, note), channel, note);
}

void PDProcessor::onNoteOff(int channel, int note, float velocity) {
  for (int32 i = static_cast<int32>(heldNotes_.size()) - 1; i >= 0; i--) {
    if (heldNotes_[i].channel == channel && heldNotes_[i].note == note) {
      heldNotes_.erase(heldNotes_.begin() + i);
    }
  }
  if (mono_) {
    if (voices_[0].isHeld(channel, note)) {
      if (!heldNotes_.empty()) {
        // Return to the most recently pressed key that is still held.
        startVoice(voices_[0], heldNotes_.back().channel, heldNotes_.back().note);
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

void PDProcessor::startVoice(Voice& voice, int channel, int note) {
  if (voice.isActive()) {
    fadeOutOldSound(voice);
  }
  if (voice.isFrozen()) {
    syncVoice(voice);  // back from the release of an old preset
  }
  voice.noteOn(channel, note, nextVoiceAge_++);
}

void PDProcessor::fadeOutOldSound(Voice& voice) {
  // The new note keeps this voice slot (and so the allocation order); the
  // old sound moves to a fade voice instead of being cut, which would click.
  // Slots are used in turn, so a busy one is the fade closest to its end.
  Voice& fade = fadeVoices_[nextFadeVoice_];
  nextFadeVoice_ = (nextFadeVoice_ + 1) % kNumFadeVoices;
  std::swap(voice, fade);
  fade.fadeOut(kFadeTicks);
}

void PDProcessor::pushScopeSample(float sample) {
  scopeFrame_[scopeFramePos_++] = sample;
  if (scopeFramePos_ < kScopeFrameSize) {
    return;
  }
  scopeFramePos_ = 0;

  DataExchangeBlock block = scopeExchange_->getCurrentOrNewBlock();
  if (block.blockID == InvalidDataExchangeBlockID) {
    return;  // queue full (or not connected): this frame is dropped
  }
  memcpy(block.data, scopeFrame_.data(), sizeof(float) * kScopeFrameSize);
  scopeExchange_->sendCurrentBlock();
}

void PDProcessor::processReplacing(ProcessData& data) {
  // The host may call process without buffers just to flush parameters.
  if (data.numOutputs == 0 || data.numSamples == 0 || data.outputs == nullptr
      || data.outputs[0].numChannels < 2 || data.outputs[0].channelBuffers32 == nullptr) {
    return;
  }

  // outputs \in [-1, 1]^(data.outputs[0].numChannels, data.numSamples)
  Sample32* outL = data.outputs[0].channelBuffers32[0];
  Sample32* outR = data.outputs[0].channelBuffers32[1];
  if (outL == nullptr || outR == nullptr) {
    return;
  }

  for (int32 i = 0; i < data.numSamples; i++) {
    Sample32 value = static_cast<Sample32>(resample());
    outL[i] = value;
    outR[i] = value;
    pushScopeSample(value);
  }
}

double PDProcessor::resample() {
  while (resamplePhase_ >= 1.0) {
    prevTickSample_ = currTickSample_;
    currTickSample_ = generate();
    resamplePhase_ -= 1.0;
  }
  double value = prevTickSample_ + (currTickSample_ - prevTickSample_) * resamplePhase_;
  resamplePhase_ += internalTickStep_;
  return value;
}

double PDProcessor::generate() {
  double mixed = 0.0;
  for (Voice& voice : voices_) {
    if (voice.isActive()) {
      mixed += voice.generate(pitchBend_);
    }
  }
  for (Voice& voice : fadeVoices_) {
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
