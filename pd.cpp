#include "pd.h"
#define _USE_MATH_DEFINES
#include <math.h>


#define PHASE_EPSILON (M_PI / 64.0)


namespace Steinberg {
namespace Vst {


SawToothPhaseGenerator::SawToothPhaseGenerator(double dcw){
    this->setDcw(dcw);
}

void SawToothPhaseGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * (1 - corrDcw);
    this->slopeLeft = 1 / (1 - corrDcw);
    this->slopeRight = 1 / (1 + corrDcw);
}

double SawToothPhaseGenerator::getPhase(double phasetime){
    if(phasetime < this->breakpoint){
        return this->slopeLeft * phasetime;
    }else{
        return M_PI + this->slopeRight * (phasetime - this->breakpoint);
    }
}


SquarePhaseGenerator::SquarePhaseGenerator(double dcw){
    this->setDcw(dcw);
}

void SquarePhaseGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * (1 - corrDcw);
    this->slopeLeft = (M_PI - PHASE_EPSILON) / (M_PI * (1 - corrDcw));
    this->slopeRight = PHASE_EPSILON / (M_PI * corrDcw);
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


PulsePhaseGenerator::PulsePhaseGenerator(double dcw){
    this->setDcw(dcw);
}

void PulsePhaseGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * corrDcw;
    this->slopeLeft = PHASE_EPSILON / (M_PI * corrDcw);
    this->slopeRight = (M_PI - PHASE_EPSILON) / (M_PI * (1 - corrDcw));
}

double PulsePhaseGenerator::getPhase(double phasetime){
    if(phasetime < this->breakpoint){
        return this->slopeLeft * phasetime;
    }else if(this->breakpoint <= phasetime && phasetime < M_PI){
        return PHASE_EPSILON + this->slopeRight * (phasetime - this->breakpoint);
    }else if(M_PI <= phasetime && phasetime < 2 * M_PI - this->breakpoint){
        return M_PI + this->slopeRight * (phasetime - M_PI);
    }else{
        return (2 * M_PI - PHASE_EPSILON) + this->slopeLeft * (phasetime - (2 * M_PI - this->breakpoint));
    }
}


DoubleSinePhaseGenerator::DoubleSinePhaseGenerator(double dcw){
    this->setDcw(dcw);
}

void DoubleSinePhaseGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * (1 - corrDcw);
    this->slopeLeft = 2 / (1 - corrDcw);
    this->slopeRight = 2 / (1 + corrDcw);
}

double DoubleSinePhaseGenerator::getPhase(double phasetime){
    if(phasetime < this->breakpoint){
        return this->slopeLeft * phasetime;
    }else{
        return 2 * M_PI + this->slopeRight * (phasetime - this->breakpoint);
    }
}


PD::PD():
    waveform(Waveform::SAWTOOTH),
    dcw(0.0),
    phasetime(0.0),
    phaseGenerator(std::make_unique<SawToothPhaseGenerator>(dcw)){
}

void PD::setWaveform(int8 waveformIndex){
    this->waveform = static_cast<Waveform>(waveformIndex);
    switch(this->waveform){
        case Waveform::SAWTOOTH:
            this->phaseGenerator = std::make_unique<SawToothPhaseGenerator>(this->dcw);
            break;
        case Waveform::SQUARE:
            this->phaseGenerator = std::make_unique<SquarePhaseGenerator>(this->dcw);
            break;
        case Waveform::PULSE:
            this->phaseGenerator = std::make_unique<PulsePhaseGenerator>(this->dcw);
            break;
        case Waveform::DOUBLE_SINE:
            this->phaseGenerator = std::make_unique<DoubleSinePhaseGenerator>(this->dcw);
            break;
        case Waveform::SAW_PULSE:
            break;
        case Waveform::RESONANCE_SAWTOOTH:
            break;
        case Waveform::RESONANCE_TRIANGLE:
            break;
        case Waveform::RESONANCE_TRAPEZOID:
            break;
        default:  // never reached
            break;
    }
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