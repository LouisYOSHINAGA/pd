#include "eg.h"
#include "const.h"


namespace Steinberg {
namespace Vst {


EgParam::EgParam():
    rates{}, levels{},
    step(EG_STEP_HALT),
    level(0.0), dLevel(0.0){
}

void EgParam::setRate(int32 index, ParamValue rate){
    this->rates[index] = rate;
}

void EgParam::setLevel(int32 index, ParamValue level){
    this->levels[index] = level;
}

void EgParam::setup(void){
    this->level = this->levels[0];
    this->proceed(1);
}

void EgParam::proceed(int8 step){
    this->step = step;
    double dSample = 5 * (1.0 - this->rates[this->step]) * SAMPLING_RATE + 10;  // TODO: temp impl
    this->dLevel = (this->levels[this->step] - this->levels[this->step-1]) / dSample;
}

void EgParam::halt(void){
    this->dLevel = 0;
    this->step = EG_STEP_HALT;
}

void EgParam::update(void){
    this->level += this->dLevel;
    if(this->dLevel > 0 && this->level >= this->levels[this->step]){
        this->level = this->levels[this->step];
        if(this->step == N_EG_STEPS){
            this->halt();
        }else{
            this->proceed(this->step + 1);
        }
    }else if(this->dLevel < 0 && this->level <= this->levels[this->step]){
        this->level = this->levels[this->step];
        if(this->step == N_EG_STEPS){
            this->halt();
        }else{
            this->proceed(this->step + 1);
        }
    }
}

double EgParam::generate(void){
    double level = this->level;
    this->update();
    return level;
}


} }
