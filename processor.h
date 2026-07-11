#pragma once

#include <array>
#include <cstdint>
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
  ParamValue pitchBend_;
  ParamValue volume_;
  std::array<Voice, kMaxVoices> voices_;
  uint64_t nextVoiceAge_;

  void processParameter(IParameterChanges* changes);
  void processEvent(IEventList* events);
  void processReplacing(ProcessData& data);
  void onNoteOn(int channel, int note, float velocity);
  void onNoteOff(int channel, int note, float velocity);
  double generate();

  // Returns a free voice if one exists, otherwise steals the oldest
  // allocated voice (release-tail or not) to make room for a new note.
  Voice* allocateVoice();
};

}  // namespace Vst
}  // namespace Steinberg
