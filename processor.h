#pragma once

#include <array>
#include <cstdint>
#include <vector>

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

 private:
  struct HeldNote {
    int channel;
    int note;
  };

  ParamValue pitchBend_ = 0.0;
  ParamValue volume_ = 0.5;
  LineSelect lineSelect_ = LineSelect::kLine1;
  bool mono_ = false;
  int detuneOctave_ = 0;
  int detuneNote_ = 0;
  int detuneFine_ = 0;
  std::array<Voice, kMaxVoices> voices_;
  uint64_t nextVoiceAge_ = 0;
  // Keys currently held on the keyboard, in press order; used for the
  // last-note priority behavior of mono (SOLO) mode.
  std::vector<HeldNote> heldNotes_;
  // Normalized value of every parameter, kept for state save/load.
  std::array<ParamValue, kNumParams> paramValues_;
  // Oscilloscope frame under construction; sent to the controller when full.
  std::array<float, kScopeFrameSize> scopeFrame_{};
  int32 scopeFramePos_ = 0;
  // Parameter changes seen in the current process() call that actually moved a
  // value; echoed to the controller at the end of the call.
  std::array<ParamSyncEntry, kNumParams> pendingSync_{};
  int32 numPendingSync_ = 0;

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
  // point shared by host automation (processParameter) and setState.
  void applyParameter(int32 paramId, ParamValue value);

  // Default normalized value of a parameter, matching the controller side.
  static ParamValue defaultParamValue(int32 paramId);

  void processParameter(IParameterChanges* changes);
  void processEvent(IEventList* events);
  void processReplacing(ProcessData& data);
  void onNoteOn(int channel, int note, float velocity);
  void onNoteOff(int channel, int note, float velocity);
  double generate();

  // Returns the next output-rate sample by linearly interpolating between
  // internal-rate (kInternalSampleRate) ticks, advancing the internal engine
  // by one tick via generate() whenever the accumulated phase demands it.
  double resample();

  // Recomputes the detune ratio from octave/note/fine and broadcasts it.
  void updateDetune();

  // Sends note-off to every sounding voice (used when switching mono/poly).
  void releaseAllVoices();

  // Polyphony available under the current line select; dual-line modes
  // halve it, as on the CZ series.
  int32 effectiveMaxVoices() const;

  // Returns a free voice if one exists, otherwise steals the oldest
  // allocated voice (release-tail or not) to make room for a new note.
  Voice* allocateVoice();
};

}  // namespace Vst
}  // namespace Steinberg
