#pragma once

#include <vector>
#include <tuple>
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "const.h"


using namespace std;
namespace Steinberg {
namespace Vst {


class NoteFreqTuple {
    private:
        int note;
        double freq;
    public:
        NoteFreqTuple(int note){
            this->note = note;
            this->freq = 440.0 * pow(2.0, (note - 69) / 12.0);
        }
        virtual double getFreq(void){
            return this->freq;
        }
        virtual bool isSameNote(int note){
            return this->note == note;
        }
};


class PDProcessor: public AudioEffect {
    protected:
        ParamValue volume;
        WaveformType waveform;
        vector<NoteFreqTuple> noteFreqListPressed;
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
        virtual void proceed(void);
        virtual double generate(void);
};


} }
