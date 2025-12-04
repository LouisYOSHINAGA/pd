#pragma once

#include <vector>
#include <tuple>
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "const.h"
#include "pd.h"


using namespace std;
namespace Steinberg{
namespace Vst{


class NoteFreqTuple{
    private:
        int note;
        double freq;
    public:
        NoteFreqTuple(int note):
            note(note),
            freq(A4_FREQ * pow(2.0, (note - A4_NOTE) / 12.0)){
        }
        virtual double getFreq(double dNote){
            return pow(2.0, dNote / 12.0) * this->freq;
        }
        virtual bool isSameNote(int note){
            return this->note == note;
        }
};


class PDProcessor: public AudioEffect{
    private:
        ParamValue pitchBend;
        ParamValue volume;
        vector<NoteFreqTuple> noteFreqListPressed;
        NoteFreqTuple* noteFreqReleased;
        PD pd;
    public:
        static FUnknown* createInstance(void*);
        PDProcessor();
        virtual void initializeParameter(void);
        tresult PLUGIN_API initialize(FUnknown*);
        tresult PLUGIN_API setBusArrangements(SpeakerArrangement*, int32, SpeakerArrangement*, int32);
        tresult PLUGIN_API process(ProcessData&);
        virtual void processParameter(IParameterChanges* const&);
        virtual void processEvent(IEventList* const&);
        virtual void processReplacing(ProcessData&);
        virtual void onNoteOn(int, int, float);
        virtual void onNoteOff(int, int, float);
        virtual double generate(void);
};


} }
