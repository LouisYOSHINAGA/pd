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

        switch(queue->getParameterId()){
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
}

void PDProcessor::onNoteOff(int channel, int note, float velocity){
    for(int16 i = this->noteFreqListPressed.size()-1; i >= 0; i--){
        if(this->noteFreqListPressed[i].isSameNote(note)){
            this->noteFreqListPressed.erase(this->noteFreqListPressed.begin()+i);
        }
    }
}

void PDProcessor::processReplacing(ProcessData& data){
    // outputs \in [-1, 1]^(data.outputs[0].numChannels, data.numSamples)
    Sample32* outL = data.outputs[0].channelBuffers32[0];
    Sample32* outR = data.outputs[0].channelBuffers32[1];

    if(this->noteFreqListPressed.size() == 0){  // no sound
        memset(outL, 0.0, sizeof(Sample32)*data.numSamples);
        memset(outR, 0.0, sizeof(Sample32)*data.numSamples);
        return;
    }

    for(int32 i = 0; i < data.numSamples; i++){
        double value = this->generate();
        outL[i] = value;
        outR[i] = value;
    }
}

double PDProcessor::generate(void){
    double freq = this->noteFreqListPressed[this->noteFreqListPressed.size()-1].getFreq();
    return this->volume * this->pd.generate(freq);
}


} }
