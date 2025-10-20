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
    if(result == kResultTrue){
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
        Parameter* dcw = new Parameter(
            STR16("DCW"),  // title
            PARAM_ID_DCW  // tag
        );
        parameters.addParameter(dcw);

        // add initialize here if needed
    }
    return result;
}


} }
