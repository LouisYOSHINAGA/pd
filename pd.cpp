#include "pd.h"
#define _USE_MATH_DEFINES
#include <math.h>


#define PHASE_EPSILON (M_PI / 64.0)


namespace Steinberg{
namespace Vst{


AbstractGenerator::AbstractGenerator():
    breakpoint(M_PI), slopeLeft(1.0), slopeRight(1.0){
}

double AbstractGenerator::generate(double phasetime){
    return - cos(this->getPhase(phasetime));
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


double ResonanceSawToothGenerator::getEnvelope(double phasetime){
    return 1 - phasetime / (2 * M_PI - PHASE_EPSILON);
}


double ResonanceTriangleGenerator::getEnvelope(double phasetime){
    if(phasetime < M_PI){
        return PHASE_EPSILON + (1 - PHASE_EPSILON) / M_PI * phasetime;
    }else{
        return PHASE_EPSILON + 1 - (1 - PHASE_EPSILON) / M_PI * (phasetime - M_PI);
    }
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
    phasetime(0.0),
    generator(std::make_unique<SawToothGenerator>()){
}

void PD::setWaveform(int8 waveformIndex){
    this->waveform = static_cast<Waveform>(waveformIndex);
    switch(this->waveform){
        case Waveform::SAWTOOTH:
            this->generator = std::make_unique<SawToothGenerator>();
            break;
        case Waveform::SQUARE:
            this->generator = std::make_unique<SquareGenerator>();
            break;
        case Waveform::PULSE:
            this->generator = std::make_unique<PulseGenerator>();
            break;
        case Waveform::DOUBLE_SINE:
            this->generator = std::make_unique<DoubleSineGenerator>();
            break;
        case Waveform::SAW_PULSE:
            this->generator = std::make_unique<SawPulseGenerator>();
            break;
        case Waveform::RESONANCE_SAWTOOTH:
            this->generator = std::make_unique<ResonanceSawToothGenerator>();
            break;
        case Waveform::RESONANCE_TRIANGLE:
            this->generator = std::make_unique<ResonanceTriangleGenerator>();
            break;
        case Waveform::RESONANCE_TRAPEZOID:
            this->generator = std::make_unique<ResonanceTrapezoidGenerator>();
            break;
        default:  // never reached
            break;
    }
}

double PD::generate(double freq, bool& isDcaEnd){
    this->phasetime += 2 * M_PI * freq * (1 + 3 * this->dcoEg.generate()) / SAMPLING_RATE;  // temp impl
    if(this->phasetime >= 2 * M_PI){
        this->phasetime -= 2 * M_PI;
    }
    this->generator->setDcw(this->dcwEg.generate());
    return this->dcaEg.generate(isDcaEnd) * this->generator->generate(this->phasetime);
}

void PD::setupEg(void){
    this->dcoEg.setup();
    this->dcwEg.setup();
    this->dcaEg.setup();
}

void PD::setEgRate(int32 paramId, int32 index, ParamValue rate){
    if(PARAM_ID_DCO_EG_RATE_0 <= paramId && paramId <= PARAM_ID_DCO_EG_RATE_7){
        this->dcoEg.setRate(index, rate);
    }else if(PARAM_ID_DCW_EG_RATE_0 <= paramId && paramId <= PARAM_ID_DCW_EG_RATE_7){
        this->dcwEg.setRate(index, rate);
    }else if(PARAM_ID_DCA_EG_RATE_0 <= paramId && paramId <= PARAM_ID_DCA_EG_RATE_7){
        this->dcaEg.setRate(index, rate);
    }
}

void PD::setEgLevel(int32 paramId, int32 index, ParamValue level){
    if(PARAM_ID_DCO_EG_LVL_0 <= paramId && paramId <= PARAM_ID_DCO_EG_LVL_6){
        this->dcoEg.setLevel(index, level);
    }else if(PARAM_ID_DCW_EG_LVL_0 <= paramId && paramId <= PARAM_ID_DCW_EG_LVL_6){
        this->dcwEg.setLevel(index, level);
    }else if(PARAM_ID_DCA_EG_LVL_0 <= paramId && paramId <= PARAM_ID_DCA_EG_LVL_6){
        this->dcaEg.setLevel(index, level);
    }
}

void PD::setEgSustainPoint(int32 paramId, int8 point){
    if(paramId == PARAM_ID_DCO_EG_SUSTAIN_POINT){
        this->dcoEg.setSustainPoint(point);
    }else if(paramId == PARAM_ID_DCW_EG_SUSTAIN_POINT){
        this->dcwEg.setSustainPoint(point);
    }else if(paramId == PARAM_ID_DCA_EG_SUSTAIN_POINT){
        this->dcaEg.setSustainPoint(point);
    }
}

void PD::setEgEndPoint(int32 paramId, int8 point){
    if(paramId == PARAM_ID_DCO_EG_END_POINT){
        this->dcoEg.setEndPoint(point);
    }else if(paramId == PARAM_ID_DCW_EG_END_POINT){
        this->dcwEg.setEndPoint(point);
    }else if(paramId == PARAM_ID_DCA_EG_END_POINT){
        this->dcaEg.setEndPoint(point);
    }
}

void PD::restartEg(void){
    this->dcoEg.restart();
    this->dcwEg.restart();
    this->dcaEg.restart();
}

void PD::haltEg(void){
    this->dcoEg.halt();
    this->dcwEg.halt();
    this->dcaEg.halt();
}


} }