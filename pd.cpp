#include "pd.h"
#define _USE_MATH_DEFINES
#include <math.h>


#define PHASE_EPSILON (M_PI / 128.0)


namespace Steinberg {
namespace Vst {


SawToothPhaseGenerator::SawToothPhaseGenerator(){
}

SawToothPhaseGenerator::SawToothPhaseGenerator(double dcw){
    this->setDcw(dcw);
}

void SawToothPhaseGenerator::setDcw(double dcw){
    this->breakpoint = M_PI * (1 - dcw);
    this->slopeLeft = 1 / (1 - dcw);
    this->slopeRight = 1 / (1 + dcw);
}

double SawToothPhaseGenerator::getPhase(double phasetime){
    if(phasetime < this->breakpoint){
        return this->slopeLeft * phasetime;
    }else{
        return this->slopeRight * (phasetime - this->breakpoint) + M_PI;
    }
}


SquarePhaseGenerator::SquarePhaseGenerator(){
}

SquarePhaseGenerator::SquarePhaseGenerator(double dcw){
    this->setDcw(dcw);
}

void SquarePhaseGenerator::setDcw(double dcw){
    this->breakpoint = M_PI * (1 - dcw);
    this->slopeLeft = (M_PI - PHASE_EPSILON) / (M_PI * (1 - dcw));
    this->slopeRight = PHASE_EPSILON / (M_PI * dcw);
}

double SquarePhaseGenerator::getPhase(double phasetime){
    if(phasetime < this->breakpoint){
        return this->slopeLeft * phasetime;
    }else if(this->breakpoint <= phasetime && phasetime < M_PI){
        return (M_PI - PHASE_EPSILON) + this->slopeRight * (phasetime - this->breakpoint);
    }else if(M_PI <= phasetime && phasetime < M_PI + this->breakpoint){
        return M_PI + this->slopeLeft * (phasetime - M_PI);
    }else{
        return (2 * M_PI - PHASE_EPSILON) + this->slopeRight * (phasetime - (M_PI + this->breakpoint));
    }
}


// PulsePhaseGenerator::PulsePhaseGenerator(){
// }

// PulsePhaseGenerator::PulsePhaseGenerator(double dcw){
//     this->setDcw(dcw);
// }

// void PulsePhaseGenerator::setDcw(double dcw){
//     this->breakpoint = M_PI * dcw;
//     this->slopeLeft = PHASE_EPSILON / (M_PI * dcw);
//     this->slopeRight = (M_PI - PHASE_EPSILON) / (M_PI * dcw);
// }

// double PulsePhaseGenerator::getPhase(double phasetime){
//     if(phasetime < this->breakpoint){
//         return this->slopeLeft * phasetime;
//     }else if(this->breakpoint <= phasetime && phasetime < M_PI){
//         return PHASE_EPSILON + this->slopeLeft * (phasetime - this->breakpoint);
//     }else if(M_PI <= phasetime && phasetime < 2 * M_PI - this->breakpoint){
//         return M_PI + this->slopeRight * (phasetime - M_PI);
//     }else{
//         return (2 * M_PI - PHASE_EPSILON) + this->slopeLeft * (phasetime - (2 * M_PI - this->breakpoint));
//     }
// }


PD::PD():
    waveform(WaveformType::WAVEFORM_SAWTOOTH),
    dcw(0.0),
    phasetime(0.0),
    sawToothPhaseGenerator(this->dcw),
    phaseGenerator(std::make_unique<SawToothPhaseGenerator>(this->dcw)){
}

void PD::setWaveform(int8 waveformIndex){
    this->waveform = static_cast<WaveformType>(waveformIndex);
    switch(this->waveform){
        case WaveformType::WAVEFORM_SAWTOOTH:
            this->phaseGenerator = std::make_unique<SawToothPhaseGenerator>(this->dcw);
            break;
        case WaveformType::WAVEFORM_SQUARE:
            this->phaseGenerator = std::make_unique<SquarePhaseGenerator>(this->dcw);
            break;
        // case WaveformType::WAVEFORM_PULSE:
        //     this->phaseGenerator =  std::make_unique<PulsePhaseGenerator>(this->dcw);
        //     break;
        // case WAVEFORM_DOUBLE_SINE:
        //     break;
        // case WAVEFORM_SAW_PULSE:
        //     break;
        // case WAVEFORM_RESONANCE_I_SAWTOOTH:
        //     break;
        // case WAVRFORM_RESONANCE_II_TRIANGLE:
        //     break;
        // case WAVEFORM_RESONANCE_III_TRAPEZOID:
        //     break;
        default:  // never reached
            break;
    }
    this->phaseGenerator->setDcw(this->dcw);
}

void PD::setDcw(void){
    this->phaseGenerator->setDcw(this->dcw);
}

void PD::setDcw(ParamValue dcw){
    this->dcw = dcw;
    this->phaseGenerator->setDcw(this->dcw);
}

double PD::generate(double freq){
    this->phasetime += 2 * M_PI * freq / SAMPLING_RATE;
    if(this->phasetime >= 2 * M_PI){
        this->phasetime -= 2 * M_PI;
    }
    return cos(this->phaseGenerator->getPhase(this->phasetime));
}


} }