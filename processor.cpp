#include "processor.h"
#include "config.h"
#include "const.h"


#define INPUT_EVENT_CHANNELS 1


namespace Steinberg {
namespace Vst {


FUnknown* PDProcessor::createInstance(void*){
    return (IAudioProcessor*)new PDProcessor();
}

PDProcessor::PDProcessor(){
    setControllerClass(ControllerUID);
    PDProcessor::initializeParameter();
}

void PDProcessor::initializeParameter(void){
    this->volume = 0.8;
    this->noteFreqListPressed.clear();
    this->noteFreqReleased = nullptr;
    this->pd = PD();
}

tresult PLUGIN_API PDProcessor::initialize(FUnknown* context){
    tresult result = AudioEffect::initialize(context);
    if(result == kResultTrue){
        addEventInput(STR16("EventInput"), INPUT_EVENT_CHANNELS);  // input: INPUT_EVENT_CHANNELS event
        addAudioOutput(STR16("AudioOutput"), SpeakerArr::kStereo);  // output: stereo audio
    }
    return result;
}

tresult PLUGIN_API PDProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts){
    if(numOuts == 1 && outputs[0] == SpeakerArr::kStereo){
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    }
    return kResultFalse;
}

tresult PLUGIN_API PDProcessor::process(ProcessData& data){
    processParameter(data.inputParameterChanges);
    processEvent(data.inputEvents);
    processReplacing(data);
    return kResultTrue;
}

void PDProcessor::processParameter(IParameterChanges* const& iParamChanges){
    if(iParamChanges == NULL){
        return;
    }

    int32 sampleOffset;
    ParamValue value;

    for(int8 i = 0; i < iParamChanges->getParameterCount(); i++){
        IParamValueQueue* queue = iParamChanges->getParameterData(i);
        if(queue == nullptr){
            continue;
        }
        if(queue->getPoint(queue->getPointCount()-1, sampleOffset, value) == kResultFalse){
            continue;
        }

        int32 paramId = queue->getParameterId();
        switch(paramId){
            case PARAM_ID_VOLUME:
                this->volume = value;
                break;
            case PARAM_ID_WAVEFORM:
                this->pd.setWaveform(
                    static_cast<int8>(value * static_cast<int8>(Waveform::N_WAVEFORMS) + EPSILON)
                );
                break;
            case PARAM_ID_DCW:
                this->pd.setDcw(value);
                break;
            case PARAM_ID_DCA_EG_RATE_1:
            case PARAM_ID_DCA_EG_RATE_2:
            case PARAM_ID_DCA_EG_RATE_3:
            case PARAM_ID_DCA_EG_RATE_4:
            case PARAM_ID_DCA_EG_RATE_5:
            case PARAM_ID_DCA_EG_RATE_6:
            case PARAM_ID_DCA_EG_RATE_7:
            case PARAM_ID_DCA_EG_RATE_8:
                this->pd.setDcaRate(paramId-PARAM_ID_DCA_EG_RATE_1+1, value);
                break;
            case PARAM_ID_DCA_EG_LVL_0:
            case PARAM_ID_DCA_EG_LVL_1:
            case PARAM_ID_DCA_EG_LVL_2:
            case PARAM_ID_DCA_EG_LVL_3:
            case PARAM_ID_DCA_EG_LVL_4:
            case PARAM_ID_DCA_EG_LVL_5:
            case PARAM_ID_DCA_EG_LVL_6:
            case PARAM_ID_DCA_EG_LVL_7:
            case PARAM_ID_DCA_EG_LVL_8:
                this->pd.setDcaLevel(paramId-PARAM_ID_DCA_EG_LVL_0, value);
                break;
            case PARAM_ID_DCA_EG_SUSTAIN_POINT:
                this->pd.setDcaSustainPoint(
                    static_cast<int8>(value * N_OPT_EG_SUSTAIN_POINT + EPSILON)
                );
                break;
            case PARAM_ID_DCA_EG_END_POINT:
                this->pd.setDcaEndPoint(
                    static_cast<int8>(value * N_OPT_EG_END_POINT + EPSILON)
                );
                break;
            default:
                // do nothing
                break;
        }
    }
}

void PDProcessor::processEvent(IEventList* const& eventList){
    if(eventList == nullptr){
        return;
    }

    int32 numEvent = eventList->getEventCount();
    Event event;
    for(int32 i = 0; i < numEvent; i++){
        if(eventList->getEvent(i, event) == kResultFalse){
            continue;
        }
        switch(event.type){
            case Event::kNoteOnEvent:
                onNoteOn(event.noteOn.channel, event.noteOn.pitch, event.noteOn.velocity);
                break;
            case Event::kNoteOffEvent:
                onNoteOff(event.noteOff.channel, event.noteOff.pitch, event.noteOff.velocity);
                break;
            default:
                // do nothing
                break;
        }
    }
}

void PDProcessor::onNoteOn(int channel, int note, float velocity){
    this->noteFreqListPressed.push_back(NoteFreqTuple(note));
    this->pd.setupEg();
}

void PDProcessor::onNoteOff(int channel, int note, float velocity){
    for(int16 i = this->noteFreqListPressed.size()-1; i >= 0; i--){
        if(this->noteFreqListPressed[i].isSameNote(note)){
            this->noteFreqListPressed.erase(this->noteFreqListPressed.begin()+i);
        }
    }
    if(this->noteFreqListPressed.size() == 0){  // last note off
        this->noteFreqReleased = new NoteFreqTuple(note);
        this->pd.restartEg();
    }
}

void PDProcessor::processReplacing(ProcessData& data){
    // outputs \in [-1, 1]^(data.outputs[0].numChannels, data.numSamples)
    Sample32* outL = data.outputs[0].channelBuffers32[0];
    Sample32* outR = data.outputs[0].channelBuffers32[1];

    for(int32 i = 0; i < data.numSamples; i++){
        if(this->noteFreqListPressed.size() == 0 && this->noteFreqReleased == nullptr){  // no sound
            memset(outL+i, 0.0, sizeof(Sample32)*(data.numSamples-i));
            memset(outR+i, 0.0, sizeof(Sample32)*(data.numSamples-i));
            return;
        }

        double value = this->generate();
        outL[i] = value;
        outR[i] = value;
    }
}

double PDProcessor::generate(void){
    double freq;
    bool isDcaEnd = false;

    if(this->noteFreqListPressed.size() > 0){
        freq = this->noteFreqListPressed[this->noteFreqListPressed.size()-1].getFreq();
    }else{
        freq = this->noteFreqReleased->getFreq();
    }

    double out = this->volume * this->pd.generate(freq, isDcaEnd);
    if(isDcaEnd){
        this->noteFreqReleased = nullptr;
    }
    return out;
}


} }
