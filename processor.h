#pragma once

#include <vector>
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "const.h"


#define INPUT_EVENT_CHANNELS 1


using namespace std;
namespace Steinberg {
namespace Vst {


class PDProcessor: public AudioEffect {
    protected:
        ParamValue volume;
        WaveformType waveform;
        vector<float> pitchList;
        ParamValue theta;
    public:
        static FUnknown* createInstance(void*);
        PDProcessor();
        virtual void initializeParameter(void);
        tresult PLUGIN_API initialize(FUnknown*);
        tresult PLUGIN_API setBusArrangements(SpeakerArrangement*, int32, SpeakerArrangement*, int32);
        tresult PLUGIN_API process(ProcessData&);
        virtual void processParameter(ProcessData&);
        virtual void processEvent(IEventList*);
        virtual void processReplacing(ProcessData&);
        virtual void onNoteOn(int, int, float);
        virtual void onNoteOff(int, int, float);
};


} }
