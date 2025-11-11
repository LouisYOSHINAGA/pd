#include "controller.h"
#include "const.h"
#include "config.h"


namespace Steinberg {
namespace Vst {


FUnknown* PDController::createInstance(void*){
    return (IEditController*)new PDController();
}


tresult PLUGIN_API PDController::initialize(FUnknown* context){
    tresult result = EditController::initialize(context);
    if(result == kResultFalse){
        return kResultFalse;
    }

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

    // DCW
    Parameter* dcw = new Parameter(STR16("DCW"), PARAM_ID_DCW);
    parameters.addParameter(dcw);

    // DCA EG
    Parameter* param;
    param = new Parameter(STR16("DCA EG Rate 1"), PARAM_ID_DCA_EG_RATE_1);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 1"), PARAM_ID_DCA_EG_LVL_1);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Rate 2"), PARAM_ID_DCA_EG_RATE_2);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 2"), PARAM_ID_DCA_EG_LVL_2);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Rate 3"), PARAM_ID_DCA_EG_RATE_3);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 3"), PARAM_ID_DCA_EG_LVL_3);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Rate 4"), PARAM_ID_DCA_EG_RATE_4);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 4"), PARAM_ID_DCA_EG_LVL_4);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Rate 5"), PARAM_ID_DCA_EG_RATE_5);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 5"), PARAM_ID_DCA_EG_LVL_5);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Rate 6"), PARAM_ID_DCA_EG_RATE_6);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 6"), PARAM_ID_DCA_EG_LVL_6);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Rate 7"), PARAM_ID_DCA_EG_RATE_7);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 7"), PARAM_ID_DCA_EG_LVL_7);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Rate 8"), PARAM_ID_DCA_EG_RATE_8);
    parameters.addParameter(param);
    param = new Parameter(STR16("DCA EG Lvl 8"), PARAM_ID_DCA_EG_LVL_8);
    parameters.addParameter(param);


    // add initialize here if needed

    return kResultTrue;
}


} }
