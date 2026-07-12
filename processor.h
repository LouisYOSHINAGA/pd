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

 private:
  struct HeldNote {
    int channel;
    int note;
  };

  ParamValue pitchBend_;
  ParamValue volume_;
  LineSelect lineSelect_;
  bool mono_;
  int detuneOctave_;
  int detuneNote_;
  int detuneFine_;
  std::array<Voice, kMaxVoices> voices_;
  uint64_t nextVoiceAge_;
  // Keys currently held on the keyboard, in press order; used for the
  // last-note priority behavior of mono (SOLO) mode.
  std::vector<HeldNote> heldNotes_;

  void processParameter(IParameterChanges* changes);
  void processEvent(IEventList* events);
  void processReplacing(ProcessData& data);
  void onNoteOn(int channel, int note, float velocity);
  void onNoteOff(int channel, int note, float velocity);
  double generate();

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
