#include "eg.h"
#include "const.h"


namespace Steinberg {
namespace Vst {


AbstractEG::AbstractEG():
    rates{}, levels{},
    sustainPoint(EG_SUSTAIN_OFF),  // default: Off
    endPoint(EG_END_POINT_OFFSET),  // default: 2
    step(EG_STEP_HALT),
    level(0.0), dLevel(0.0), target(0.0){
}

void AbstractEG::setRate(int32 index, ParamValue rate){
    this->rates[index] = rate;
}

void AbstractEG::setLevel(int32 index, ParamValue level){
    this->levels[index] = level;
}

void AbstractEG::setSustainPoint(int8 point){
    this->sustainPoint = EG_SUSTAIN_POINT_OFFSET + point;
    if(this->sustainPoint == 0){  // Off
        this->sustainPoint = EG_SUSTAIN_OFF;
    }
}

void AbstractEG::setEndPoint(int8 point){
    this->endPoint = EG_END_POINT_OFFSET + point;
}

void AbstractEG::setup(void){
    this->level = this->levels[0];
    this->proceed(0);
}

void AbstractEG::update(void){
    this->level += this->dLevel;
    if(this->step == this->endPoint && this->dLevel < 0 && this->level <= 0){
        this->dLevel = 0;
        this->step = EG_STEP_HALT;
    }else if((this->dLevel > 0 && this->level >= this->target)
          || (this->dLevel < 0 && this->level <= this->target)){
        this->level = this->levels[this->step];
        this->proceed(this->step);
    }
}

void AbstractEG::restart(void){
    if(this->sustainPoint + 1 >= this->endPoint){
        return;
    }
    this->step = this->sustainPoint + 1;
    double dSample = 5 * (1.0 - this->rates[this->step]) * SAMPLING_RATE + 10;  // TODO: temp impl
    this->dLevel = (this->levels[this->step] - this->levels[this->step-1]) / dSample;
}

double AbstractEG::generate(void){
    double level = this->level;
    this->update();
    return level;
}


ZeroEndEG::ZeroEndEG(){
}

void ZeroEndEG::proceed(int8 step){
    if(step == this->sustainPoint){
        this->dLevel = 0;
        this->step = EG_STEP_SUSTAIN;
        return;
    }

    if(step == this->endPoint - 1){
        this->target = 0;  // DCA target level in end point must be 0
    }else{
        this->target = this->levels[step+1];
    }
    double dSample = 5 * (1.0 - this->rates[step+1]) * SAMPLING_RATE + 10;  // TODO: temp impl
    this->dLevel = (this->target - this->level) / dSample;
    this->step = step + 1;
}


} }
