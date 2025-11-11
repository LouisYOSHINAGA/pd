#include "eg.h"


namespace Steinberg {
namespace Vst {


EgParam::EgParam(){
}

void EgParam::setRate(int32 index, ParamValue rate){
    this->rates[index] = rate;
}

void EgParam::setLevel(int32 index, ParamValue level){
    this->levels[index] = level;
}


} }
