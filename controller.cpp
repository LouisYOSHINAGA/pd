#include "controller.h"

#include "const.h"
#include "config.h"

namespace Steinberg {
namespace Vst {

FUnknown* PDController::createInstance(void*) {
  return (IEditController*)new PDController();
}

tresult PLUGIN_API PDController::initialize(FUnknown* context) {
  tresult result = EditController::initialize(context);
  if (result == kResultFalse) {
    return kResultFalse;
  }

  // pitch bend
  Parameter* pitchBend = new Parameter(STR16("Pitch Bend"),  // title
                                        kParamPitchBend,      // tag
                                        nullptr,              // units
                                        0.5                   // default value (normalized)
  );
  parameters.addParameter(pitchBend);

  // volume
  Parameter* volume = new Parameter(STR16("volume"),  // title
                                     kParamVolume,     // tag
                                     nullptr,          // units
                                     0.8               // default value (normalized)
  );
  parameters.addParameter(volume);

  // waveform
  StringListParameter* waveform = new StringListParameter(STR16("waveform"),  // title
                                                           kParamWaveform     // tag
  );
  waveform->appendString(STR16("1: Saw Tooth"));
  waveform->appendString(STR16("2: Square"));
  waveform->appendString(STR16("3: Pulse"));
  waveform->appendString(STR16("4: Double Sine"));
  waveform->appendString(STR16("5: Saw Pulse"));
  waveform->appendString(STR16("6: Resonance I Saw Tooth"));
  waveform->appendString(STR16("7: Resonance II Triangle"));
  waveform->appendString(STR16("8: Resonance III Trapezoid"));
  parameters.addParameter(waveform);

  // DCO EG Level & Rate
  DiscreteRangeParameter* dcoEgLR;
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 1"), kParamDcoEgRate0);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 1"), kParamDcoEgLevel0);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 2"), kParamDcoEgRate1);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 2"), kParamDcoEgLevel1);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 3"), kParamDcoEgRate2);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 3"), kParamDcoEgLevel2);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 4"), kParamDcoEgRate3);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 4"), kParamDcoEgLevel3);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 5"), kParamDcoEgRate4);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 5"), kParamDcoEgLevel4);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 6"), kParamDcoEgRate5);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 6"), kParamDcoEgLevel5);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 7"), kParamDcoEgRate6);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 7"), kParamDcoEgLevel6);
  parameters.addParameter(dcoEgLR);
  dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 8"), kParamDcoEgRate7);
  parameters.addParameter(dcoEgLR);

  // DCO EG Sustain Point
  StringListParameter* dcoEgSustainPoint =
      new StringListParameter(STR16("DCO EG Sustain Point"),  // title
                               kParamDcoEgSustainPoint         // tag
      );
  dcoEgSustainPoint->appendString(STR16("Off"));
  dcoEgSustainPoint->appendString(STR16("1"));
  dcoEgSustainPoint->appendString(STR16("2"));
  dcoEgSustainPoint->appendString(STR16("3"));
  dcoEgSustainPoint->appendString(STR16("4"));
  dcoEgSustainPoint->appendString(STR16("5"));
  dcoEgSustainPoint->appendString(STR16("6"));
  dcoEgSustainPoint->appendString(STR16("7"));
  parameters.addParameter(dcoEgSustainPoint);

  // DCO EG End Point
  StringListParameter* dcoEgEndPoint = new StringListParameter(STR16("DCO EG End Point"),  // title
                                                                kParamDcoEgEndPoint         // tag
  );
  dcoEgEndPoint->appendString(STR16("2"));
  dcoEgEndPoint->appendString(STR16("3"));
  dcoEgEndPoint->appendString(STR16("4"));
  dcoEgEndPoint->appendString(STR16("5"));
  dcoEgEndPoint->appendString(STR16("6"));
  dcoEgEndPoint->appendString(STR16("7"));
  dcoEgEndPoint->appendString(STR16("8"));
  parameters.addParameter(dcoEgEndPoint);

  // DCW EG Level & Rate
  DiscreteRangeParameter* dcwEgLR;
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 1"), kParamDcwEgRate0);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 1"), kParamDcwEgLevel0);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 2"), kParamDcwEgRate1);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 2"), kParamDcwEgLevel1);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 3"), kParamDcwEgRate2);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 3"), kParamDcwEgLevel2);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 4"), kParamDcwEgRate3);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 4"), kParamDcwEgLevel3);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 5"), kParamDcwEgRate4);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 5"), kParamDcwEgLevel4);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 6"), kParamDcwEgRate5);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 6"), kParamDcwEgLevel5);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 7"), kParamDcwEgRate6);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 7"), kParamDcwEgLevel6);
  parameters.addParameter(dcwEgLR);
  dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 8"), kParamDcwEgRate7);
  parameters.addParameter(dcwEgLR);

  // DCW EG Sustain Point
  StringListParameter* dcwEgSustainPoint =
      new StringListParameter(STR16("DCW EG Sustain Point"),  // title
                               kParamDcwEgSustainPoint         // tag
      );
  dcwEgSustainPoint->appendString(STR16("Off"));
  dcwEgSustainPoint->appendString(STR16("1"));
  dcwEgSustainPoint->appendString(STR16("2"));
  dcwEgSustainPoint->appendString(STR16("3"));
  dcwEgSustainPoint->appendString(STR16("4"));
  dcwEgSustainPoint->appendString(STR16("5"));
  dcwEgSustainPoint->appendString(STR16("6"));
  dcwEgSustainPoint->appendString(STR16("7"));
  parameters.addParameter(dcwEgSustainPoint);

  // DCW EG End Point
  StringListParameter* dcwEgEndPoint = new StringListParameter(STR16("DCW EG End Point"),  // title
                                                                kParamDcwEgEndPoint         // tag
  );
  dcwEgEndPoint->appendString(STR16("2"));
  dcwEgEndPoint->appendString(STR16("3"));
  dcwEgEndPoint->appendString(STR16("4"));
  dcwEgEndPoint->appendString(STR16("5"));
  dcwEgEndPoint->appendString(STR16("6"));
  dcwEgEndPoint->appendString(STR16("7"));
  dcwEgEndPoint->appendString(STR16("8"));
  parameters.addParameter(dcwEgEndPoint);

  // DCA EG Level & Rate
  DiscreteRangeParameter* dcaEgLR;
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 1"), kParamDcaEgRate0);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 1"), kParamDcaEgLevel0);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 2"), kParamDcaEgRate1);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 2"), kParamDcaEgLevel1);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 3"), kParamDcaEgRate2);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 3"), kParamDcaEgLevel2);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 4"), kParamDcaEgRate3);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 4"), kParamDcaEgLevel3);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 5"), kParamDcaEgRate4);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 5"), kParamDcaEgLevel4);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 6"), kParamDcaEgRate5);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 6"), kParamDcaEgLevel5);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 7"), kParamDcaEgRate6);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 7"), kParamDcaEgLevel6);
  parameters.addParameter(dcaEgLR);
  dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 8"), kParamDcaEgRate7);
  parameters.addParameter(dcaEgLR);

  // DCA EG Sustain Point
  StringListParameter* dcaEgSustainPoint =
      new StringListParameter(STR16("DCA EG Sustain Point"),  // title
                               kParamDcaEgSustainPoint         // tag
      );
  dcaEgSustainPoint->appendString(STR16("Off"));
  dcaEgSustainPoint->appendString(STR16("1"));
  dcaEgSustainPoint->appendString(STR16("2"));
  dcaEgSustainPoint->appendString(STR16("3"));
  dcaEgSustainPoint->appendString(STR16("4"));
  dcaEgSustainPoint->appendString(STR16("5"));
  dcaEgSustainPoint->appendString(STR16("6"));
  dcaEgSustainPoint->appendString(STR16("7"));
  parameters.addParameter(dcaEgSustainPoint);

  // DCA EG End Point
  StringListParameter* dcaEgEndPoint = new StringListParameter(STR16("DCA EG End Point"),  // title
                                                                kParamDcaEgEndPoint         // tag
  );
  dcaEgEndPoint->appendString(STR16("2"));
  dcaEgEndPoint->appendString(STR16("3"));
  dcaEgEndPoint->appendString(STR16("4"));
  dcaEgEndPoint->appendString(STR16("5"));
  dcaEgEndPoint->appendString(STR16("6"));
  dcaEgEndPoint->appendString(STR16("7"));
  dcaEgEndPoint->appendString(STR16("8"));
  parameters.addParameter(dcaEgEndPoint);

  // add initialize here if needed

  return kResultTrue;
}

tresult PLUGIN_API PDController::getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                               CtrlNumber midiControllerNumber,
                                                               ParamID& id) {
  switch (midiControllerNumber) {
    case kPitchBend:
      id = kParamPitchBend;
      break;
    default:
      return kResultFalse;
  }

  return kResultTrue;
}

}  // namespace Vst
}  // namespace Steinberg
