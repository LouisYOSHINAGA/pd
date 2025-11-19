#include "eg.h"
#include "const.h"


namespace Steinberg {
namespace Vst {


EgParam::EgParam():
    rates{}, levels{},
    sustainPoint(N_EG_STEPS), endPoint(N_EG_STEPS), step(EG_STEP_HALT),
    level(0.0), dLevel(0.0), target(0.0){
}

void EgParam::setRate(int32 index, ParamValue rate){
    this->rates[index] = rate;
}

void EgParam::setLevel(int32 index, ParamValue level){
    this->levels[index] = level;
}

void EgParam::setSustainPoint(int8 point){
    if(point == 0){  // off
        this->sustainPoint = N_EG_STEPS;
    }else{
        this->sustainPoint = EG_SUSTAIN_POINT_OFFSET + point;
    }
}

void EgParam::setEndPoint(int8 point){
    this->endPoint = EG_END_POINT_OFFSET + point;
}

void EgParam::setup(void){
    this->level = this->levels[0];
    this->proceed(0);
}

void EgParam::proceed(int8 step){
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

void EgParam::update(void){
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

void EgParam::restart(void){
    if(this->sustainPoint + 1 >= this->endPoint){
        return;
    }
    this->step = this->sustainPoint + 1;
    double dSample = 5 * (1.0 - this->rates[this->step]) * SAMPLING_RATE + 10;  // TODO: temp impl
    this->dLevel = (this->levels[this->step] - this->levels[this->step-1]) / dSample;
}

double EgParam::generate(void){
    double level = this->level;
    this->update();
    return level;
}


} }
