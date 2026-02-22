#include "controller.h"
#include "const.h"
#include "config.h"


namespace Steinberg{
namespace Vst{


FUnknown* PDController::createInstance(void*){
    return (IEditController*)new PDController();
}


tresult PLUGIN_API PDController::initialize(FUnknown* context){
    tresult result = EditController::initialize(context);
    if(result == kResultFalse){
        return kResultFalse;
    }

    // pitch bend
    Parameter* pitchBend = new Parameter(
        STR16("Pitch Bend"),  // title
        PARAM_ID_PITCH_BEND,  // tag
        nullptr,  // units
        0.5  // default value (normalized)
    );
    parameters.addParameter(pitchBend);

    // volume
    Parameter* volume = new Parameter(
        STR16("volume"),  // title
        PARAM_ID_VOLUME,  // tag
        nullptr,  // units
        0.8  // default value (normalized)
    );
    parameters.addParameter(volume);

    // waveform
    StringListParameter* waveform = new StringListParameter(
        STR16("waveform"),  // title
        PARAM_ID_WAVEFORM  // tag
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
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 1"), PARAM_ID_DCO_EG_RATE_0);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 1"), PARAM_ID_DCO_EG_LVL_0);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 2"), PARAM_ID_DCO_EG_RATE_1);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 2"), PARAM_ID_DCO_EG_LVL_1);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 3"), PARAM_ID_DCO_EG_RATE_2);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 3"), PARAM_ID_DCO_EG_LVL_2);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 4"), PARAM_ID_DCO_EG_RATE_3);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 4"), PARAM_ID_DCO_EG_LVL_3);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 5"), PARAM_ID_DCO_EG_RATE_4);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 5"), PARAM_ID_DCO_EG_LVL_4);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 6"), PARAM_ID_DCO_EG_RATE_5);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 6"), PARAM_ID_DCO_EG_LVL_5);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 7"), PARAM_ID_DCO_EG_RATE_6);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Lvl 7"), PARAM_ID_DCO_EG_LVL_6);
    parameters.addParameter(dcoEgLR);
    dcoEgLR = new DiscreteRangeParameter(STR16("DCO EG Rate 8"), PARAM_ID_DCO_EG_RATE_7);
    parameters.addParameter(dcoEgLR);

    // DCO EG Sustain Point
    StringListParameter* dcoEgSustainPoint = new StringListParameter(
        STR16("DCO EG Sustain Point"),  // title
        PARAM_ID_DCO_EG_SUSTAIN_POINT  // tag
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
    StringListParameter* dcoEgEndPoint = new StringListParameter(
        STR16("DCO EG End Point"),  // title
        PARAM_ID_DCO_EG_END_POINT  // tag
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
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 1"), PARAM_ID_DCW_EG_RATE_0);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 1"), PARAM_ID_DCW_EG_LVL_0);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 2"), PARAM_ID_DCW_EG_RATE_1);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 2"), PARAM_ID_DCW_EG_LVL_1);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 3"), PARAM_ID_DCW_EG_RATE_2);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 3"), PARAM_ID_DCW_EG_LVL_2);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 4"), PARAM_ID_DCW_EG_RATE_3);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 4"), PARAM_ID_DCW_EG_LVL_3);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 5"), PARAM_ID_DCW_EG_RATE_4);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 5"), PARAM_ID_DCW_EG_LVL_4);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 6"), PARAM_ID_DCW_EG_RATE_5);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 6"), PARAM_ID_DCW_EG_LVL_5);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 7"), PARAM_ID_DCW_EG_RATE_6);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Lvl 7"), PARAM_ID_DCW_EG_LVL_6);
    parameters.addParameter(dcwEgLR);
    dcwEgLR = new DiscreteRangeParameter(STR16("DCW EG Rate 8"), PARAM_ID_DCW_EG_RATE_7);
    parameters.addParameter(dcwEgLR);

    // DCW EG Sustain Point
    StringListParameter* dcwEgSustainPoint = new StringListParameter(
        STR16("DCW EG Sustain Point"),  // title
        PARAM_ID_DCW_EG_SUSTAIN_POINT  // tag
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
    StringListParameter* dcwEgEndPoint = new StringListParameter(
        STR16("DCW EG End Point"),  // title
        PARAM_ID_DCW_EG_END_POINT  // tag
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
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 1"), PARAM_ID_DCA_EG_RATE_0);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 1"), PARAM_ID_DCA_EG_LVL_0);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 2"), PARAM_ID_DCA_EG_RATE_1);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 2"), PARAM_ID_DCA_EG_LVL_1);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 3"), PARAM_ID_DCA_EG_RATE_2);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 3"), PARAM_ID_DCA_EG_LVL_2);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 4"), PARAM_ID_DCA_EG_RATE_3);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 4"), PARAM_ID_DCA_EG_LVL_3);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 5"), PARAM_ID_DCA_EG_RATE_4);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 5"), PARAM_ID_DCA_EG_LVL_4);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 6"), PARAM_ID_DCA_EG_RATE_5);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 6"), PARAM_ID_DCA_EG_LVL_5);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 7"), PARAM_ID_DCA_EG_RATE_6);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Lvl 7"), PARAM_ID_DCA_EG_LVL_6);
    parameters.addParameter(dcaEgLR);
    dcaEgLR = new DiscreteRangeParameter(STR16("DCA EG Rate 8"), PARAM_ID_DCA_EG_RATE_7);
    parameters.addParameter(dcaEgLR);

    // DCA EG Sustain Point
    StringListParameter* dcaEgSustainPoint = new StringListParameter(
        STR16("DCA EG Sustain Point"),  // title
        PARAM_ID_DCA_EG_SUSTAIN_POINT  // tag
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
    StringListParameter* dcaEgEndPoint = new StringListParameter(
        STR16("DCA EG End Point"),  // title
        PARAM_ID_DCA_EG_END_POINT  // tag
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
                                                             ParamID& id){
    switch(midiControllerNumber){
        case kPitchBend:
            id = PARAM_ID_PITCH_BEND;
            break;
        default:
            return kResultFalse;
    }

    return kResultTrue;
}


} }
