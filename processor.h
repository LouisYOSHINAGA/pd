#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "public.sdk/source/vst/utility/dataexchange.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "const.h"
#include "voice.h"

namespace Steinberg {
namespace Vst {

class PDProcessor : public AudioEffect {
 public:
  static FUnknown* createInstance(void*);
  PDProcessor();

  void initializeParameter();
  tresult PLUGIN_API initialize(FUnknown* context) override;
  tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override;
  tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                        SpeakerArrangement* outputs, int32 numOuts) override;
  tresult PLUGIN_API process(ProcessData& data) override;
  tresult PLUGIN_API getState(IBStream* state) override;
  tresult PLUGIN_API setState(IBStream* state) override;
  tresult PLUGIN_API notify(IMessage* message) override;
  tresult PLUGIN_API connect(IConnectionPoint* other) override;
  tresult PLUGIN_API disconnect(IConnectionPoint* other) override;
  tresult PLUGIN_API setActive(TBool state) override;

 private:
  // Spare voices that fade out the old sound of a voice taken over for a new
  // note (same key again, stolen voice, mono note change); a fade lasts
  // kFadeTicks internal samples, and with this many slots one is always free
  // unless more voices than that are taken over within one fade.
  static constexpr int kNumFadeVoices = 8;
  static constexpr int kFadeTicks = static_cast<int>(kInternalSampleRate * 0.004);

  struct HeldNote {
    int channel;
    int note;
  };

  ParamValue pitchBend_ = 0.0;
  ParamValue volume_ = 0.5;
  bool mono_ = false;
  // Decoded per-voice settings (applied by applyToVoice).
  LineSelect lineSelect_ = LineSelect::kLine1;
  int detuneOctave_ = 0;
  int detuneNote_ = 0;
  int detuneFine_ = 0;
  double detuneRatio_ = 1.0;
  int octaveRange_ = 0;
  int masterTuneCents_ = 0;
  std::array<int8, 2> dcaKeyFollow_{};
  std::array<int8, 2> dcwKeyFollow_{};
  std::array<Voice, kMaxVoices> voices_;
  std::array<Voice, kNumFadeVoices> fadeVoices_;
  int nextFadeVoice_ = 0;
  uint64_t nextVoiceAge_ = 0;
  // Keys currently held on the keyboard, in press order: mono (SOLO) mode's
  // last-note priority returns to them, and a preset change starts them
  // again with the new preset.
  std::vector<HeldNote> heldNotes_;
  std::vector<HeldNote> retriggerNotes_;  // scratch of changePreset()
  // Normalized value of every parameter, kept for state save/load.
  std::array<ParamValue, kNumParams> paramValues_;
  // Oscilloscope frame under construction; sent to the controller when full.
  std::array<float, kScopeFrameSize> scopeFrame_{};
  int32 scopeFramePos_ = 0;
  // Parameter changes that actually moved a value, waiting to be echoed to the
  // controller (kept until a block of the queue is free).
  std::array<bool, kNumParams> syncPending_{};
  std::array<ParamValue, kNumParams> syncValues_{};
  bool anySyncPending_ = false;
  // audio thread -> controller (see kScopeExchangeId, kParamSyncExchangeId)
  std::unique_ptr<DataExchangeHandler> scopeExchange_;
  std::unique_ptr<DataExchangeHandler> paramSyncExchange_;
  // A preset loaded by the host (setState) or by the controller (message),
  // waiting to be applied by process() on the audio thread.
  std::mutex pendingPresetMutex_;
  std::array<ParamValue, kNumParams> pendingPreset_{};
  bool hasPendingPreset_ = false;  // guarded by pendingPresetMutex_
  std::array<ParamValue, kNumParams> presetValues_{};  // audio thread's copy

  // Linear-interpolation resampler state: advances the internal engine at a
  // fixed kInternalSampleRate regardless of the host's output sample rate.
  // internalTickStep_ is kInternalSampleRate / (host sample rate); phase_
  // starts at 1.0 so the very first output sample triggers a tick.
  double internalTickStep_ = 1.0;
  double resamplePhase_ = 1.0;
  double prevTickSample_ = 0.0;
  double currTickSample_ = 0.0;

  // Accumulates one output sample for the editor's oscilloscope and sends
  // the frame to the controller whenever it is full.
  void pushScopeSample(float sample);

  // Sends the parameter changes collected by processParameter to the
  // controller, so the UI follows values that only reach the processor
  // (MIDI CC mapped parameters).
  void sendParamSync();


  // Stores and dispatches one normalized parameter value; the single entry
  // point shared by host automation (processParameter) and preset changes.
  // Frozen voices (see Voice::isFrozen) are left out.
  void applyParameter(int32 paramId, ParamValue value);
  // Applies the current value of one per-voice parameter to `voice`.
  void applyToVoice(Voice& voice, int32 paramId) const;
  // Brings a frozen voice up to the current parameters and unfreezes it.
  void syncVoice(Voice& voice);

  // Preset changes: queued from any thread, applied at the start of process().
  void queuePreset(const std::array<ParamValue, kNumParams>& values);
  void applyPendingPreset();
  // As on the CZ-101: what the old preset is sounding is released with its
  // old sound (the voices are frozen), and the keys still held start again
  // with the new preset.
  void changePreset(const std::array<ParamValue, kNumParams>& values);

  // Default normalized value of a parameter, matching the controller side.
  static ParamValue defaultParamValue(int32 paramId);

  void processParameter(IParameterChanges* changes);
  void processEvent(IEventList* events);
  void processReplacing(ProcessData& data);
  void onNoteOn(int channel, int note, float velocity);
  void onNoteOff(int channel, int note, float velocity);
  // Starts a note on `voice`, bringing it up to date first if it is frozen.
  // A voice still sounding hands its old sound to a fade voice first.
  void startVoice(Voice& voice, int channel, int note);
  void fadeOutOldSound(Voice& voice);
  double generate();

  // Returns the next output-rate sample by linearly interpolating between
  // internal-rate (kInternalSampleRate) ticks, advancing the internal engine
  // by one tick via generate() whenever the accumulated phase demands it.
  double resample();

  // Recomputes the detune ratio from octave/note/fine.
  void updateDetune();

  // Sends note-off to every sounding voice.
  void releaseAllVoices();
  // Releases every voice and forgets the held keys (mono/poly switch).
  void allNotesOff();

  // Polyphony available under the current line select; dual-line modes
  // halve it, as on the CZ series.
  int32 effectiveMaxVoices() const;

  // Returns the voice for a note-on: the voice still sounding the same
  // channel/note if any (so repeating a key never takes another voice; not a
  // frozen one, which belongs to an old preset), else a free voice, else the
  // oldest voice in its release tail, and only if all voices are held, the
  // oldest held one.
  Voice* allocateVoice(int channel, int note);
};

}  // namespace Vst
}  // namespace Steinberg
