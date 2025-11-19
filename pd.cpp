#include "pd.h"
#define _USE_MATH_DEFINES
#include <math.h>


#define PHASE_EPSILON (M_PI / 64.0)


namespace Steinberg {
namespace Vst {


double AbstractGenerator::generate(double phasetime){
    return - cos(this->getPhase(phasetime));
}


SawToothGenerator::SawToothGenerator(double dcw){
    this->setDcw(dcw);
}

void SawToothGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * (1 - corrDcw);
    this->slopeLeft = 1 / (1 - corrDcw);
    this->slopeRight = 1 / (1 + corrDcw);
}

double SawToothGenerator::getPhase(double phasetime){
    if(phasetime < this->breakpoint){
        return this->slopeLeft * phasetime;
    }else{
        return M_PI + this->slopeRight * (phasetime - this->breakpoint);
    }
}


SquareGenerator::SquareGenerator(double dcw){
    this->setDcw(dcw);
}

void SquareGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * (1 - corrDcw);
    this->slopeLeft = (M_PI - PHASE_EPSILON) / (M_PI * (1 - corrDcw));
    this->slopeRight = PHASE_EPSILON / (M_PI * corrDcw);
}

double SquareGenerator::getPhase(double phasetime){
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


PulseGenerator::PulseGenerator(double dcw){
    this->setDcw(dcw);
}

void PulseGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * corrDcw;
    this->slopeLeft = PHASE_EPSILON / (M_PI * corrDcw);
    this->slopeRight = (M_PI - PHASE_EPSILON) / (M_PI * (1 - corrDcw));
}

double PulseGenerator::getPhase(double phasetime){
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


DoubleSineGenerator::DoubleSineGenerator(double dcw){
    this->setDcw(dcw);
}

void DoubleSineGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * (1 - corrDcw);
    this->slopeLeft = 2 / (1 - corrDcw);
    this->slopeRight = 2 / (1 + corrDcw);
}

double DoubleSineGenerator::getPhase(double phasetime){
    if(phasetime < this->breakpoint){
        return this->slopeLeft * phasetime;
    }else{
        return 2 * M_PI + this->slopeRight * (phasetime - this->breakpoint);
    }
}


SawPulseGenerator::SawPulseGenerator(double dcw){
    this->setDcw(dcw);
}

void SawPulseGenerator::setDcw(double dcw){
    double corrDcw = DCW_CORRECT_COEF * dcw;
    this->breakpoint = M_PI * (1 - corrDcw);
    this->slopeLeft = (M_PI - PHASE_EPSILON) / (M_PI * (1 - corrDcw));
    this->slopeRight = PHASE_EPSILON / (1 + corrDcw);
}

double SawPulseGenerator::getPhase(double phasetime){
    if(phasetime < M_PI){
        return phasetime;
    }else if(M_PI <= phasetime && phasetime < M_PI + this->breakpoint){
        return M_PI + this->slopeLeft * (phasetime - M_PI);
    }else{
        return (2 * M_PI - PHASE_EPSILON) + this->slopeRight * (phasetime - this->breakpoint);
    }
}


void AbstractResonanceGenerator::setDcw(double dcw){
    this->highFreqPhaseCoef = 1 + MAX_FREQ_MULT * dcw;
}

double AbstractResonanceGenerator::getPhase(double phasetime){
    return phasetime;
}

double AbstractResonanceGenerator::generate(double phasetime){
    return this->getEnvelope(phasetime) * (- cos(this->highFreqPhaseCoef * phasetime) + 1) - 1;
}


ResonanceSawToothGenerator::ResonanceSawToothGenerator(double dcw){
    this->setDcw(dcw);
}

double ResonanceSawToothGenerator::getEnvelope(double phasetime){
    return 1 - phasetime / (2 * M_PI - PHASE_EPSILON);
}


ResonanceTriangleGenerator::ResonanceTriangleGenerator(double dcw){
    this->setDcw(dcw);
}

double ResonanceTriangleGenerator::getEnvelope(double phasetime){
    if(phasetime < M_PI){
        return PHASE_EPSILON + (1 - PHASE_EPSILON) / M_PI * phasetime;
    }else{
        return PHASE_EPSILON + 1 - (1 - PHASE_EPSILON) / M_PI * (phasetime - M_PI);
    }
}


ResonanceTrapezoidGenerator::ResonanceTrapezoidGenerator(double dcw){
    this->setDcw(dcw);
}

double ResonanceTrapezoidGenerator::getEnvelope(double phasetime){
    if(phasetime < M_PI){
        return 1.0;
    }else{
        return 1 - (phasetime - M_PI) / (M_PI - PHASE_EPSILON);
    }
}


PD::PD():
    waveform(Waveform::SAWTOOTH),
    dcw(0.0),
    phasetime(0.0),
    generator(std::make_unique<SawToothGenerator>(dcw)){
}

void PD::setWaveform(int8 waveformIndex){
    this->waveform = static_cast<Waveform>(waveformIndex);
    switch(this->waveform){
        case Waveform::SAWTOOTH:
            this->generator = std::make_unique<SawToothGenerator>(this->dcw);
            break;
        case Waveform::SQUARE:
            this->generator = std::make_unique<SquareGenerator>(this->dcw);
            break;
        case Waveform::PULSE:
            this->generator = std::make_unique<PulseGenerator>(this->dcw);
            break;
        case Waveform::DOUBLE_SINE:
            this->generator = std::make_unique<DoubleSineGenerator>(this->dcw);
            break;
        case Waveform::SAW_PULSE:
            this->generator = std::make_unique<SawPulseGenerator>(this->dcw);
            break;
        case Waveform::RESONANCE_SAWTOOTH:
            this->generator = std::make_unique<ResonanceSawToothGenerator>(this->dcw);
            break;
        case Waveform::RESONANCE_TRIANGLE:
            this->generator = std::make_unique<ResonanceTriangleGenerator>(this->dcw);
            break;
        case Waveform::RESONANCE_TRAPEZOID:
            this->generator = std::make_unique<ResonanceTrapezoidGenerator>(this->dcw);
            break;
        default:  // never reached
            break;
    }
}

void PD::setDcw(ParamValue dcw){
    this->dcw = dcw;
    this->generator->setDcw(this->dcw);
}

double PD::generate(double freq){
    this->phasetime += 2 * M_PI * freq / SAMPLING_RATE;
    if(this->phasetime >= 2 * M_PI){
        this->phasetime -= 2 * M_PI;
    }
    return this->dcaEg.generate() * this->generator->generate(this->phasetime);
}

void PD::setupEg(void){
    this->dcaEg.setup();
}

void PD::setDcaRate(int32 index, ParamValue rate){
    this->dcaEg.setRate(index, rate);
}

void PD::setDcaLevel(int32 index, ParamValue level){
    this->dcaEg.setLevel(index, level);
}

void PD::setDcaSustainPoint(int8 point){
    this->dcaEg.setSustainPoint(point);
}

void PD::setDcaEndPoint(int8 point){
    this->dcaEg.setEndPoint(point);
}

void PD::restartEg(void){
    this->dcaEg.restart();
}


} }