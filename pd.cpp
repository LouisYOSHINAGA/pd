#include "pd.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace Steinberg {
namespace Vst {

namespace {
constexpr double kPhaseEpsilon = M_PI / 64.0;
}  // namespace

AbstractGenerator::AbstractGenerator()
    : breakpoint_(M_PI), slopeLeft_(1.0), slopeRight_(1.0), epsilon_(0.0) {
}

double AbstractGenerator::generate(double phasetime) {
  return -cos(getPhase(phasetime));
}

void SawToothGenerator::setDcw(double dcw) {
  double corrDcw = kDcwCorrectCoef * dcw;
  breakpoint_ = M_PI * (1 - corrDcw);
  slopeLeft_ = 1 / (1 - corrDcw);
  slopeRight_ = 1 / (1 + corrDcw);
}

double SawToothGenerator::getPhase(double phasetime) {
  if (phasetime < breakpoint_) {
    return slopeLeft_ * phasetime;
  } else {
    return M_PI + slopeRight_ * (phasetime - breakpoint_);
  }
}

void SquareGenerator::setDcw(double dcw) {
  double corrDcw = kDcwCorrectCoef * dcw;
  breakpoint_ = M_PI * (1 - corrDcw);
  epsilon_ = kPhaseEpsilon * corrDcw;
  slopeLeft_ = (M_PI - epsilon_) / (M_PI * (1 - corrDcw));
  slopeRight_ = kPhaseEpsilon / M_PI;
}

double SquareGenerator::getPhase(double phasetime) {
  if (phasetime < breakpoint_) {
    return slopeLeft_ * phasetime;
  } else if (breakpoint_ <= phasetime && phasetime < M_PI) {
    return (M_PI - epsilon_) + slopeRight_ * (phasetime - breakpoint_);
  } else if (M_PI <= phasetime && phasetime < M_PI + breakpoint_) {
    return M_PI + slopeLeft_ * (phasetime - M_PI);
  } else {
    return (2 * M_PI - epsilon_) + slopeRight_ * (phasetime - (M_PI + breakpoint_));
  }
}

void PulseGenerator::setDcw(double dcw) {
  double corrDcw = kDcwCorrectCoef * dcw;
  breakpoint_ = M_PI * corrDcw;
  epsilon_ = kPhaseEpsilon * corrDcw;
  slopeLeft_ = kPhaseEpsilon / M_PI;
  slopeRight_ = (M_PI - epsilon_) / (M_PI * (1 - corrDcw));
}

double PulseGenerator::getPhase(double phasetime) {
  if (phasetime < breakpoint_) {
    return slopeLeft_ * phasetime;
  } else if (breakpoint_ <= phasetime && phasetime < M_PI) {
    return epsilon_ + slopeRight_ * (phasetime - breakpoint_);
  } else if (M_PI <= phasetime && phasetime < 2 * M_PI - breakpoint_) {
    return M_PI + slopeRight_ * (phasetime - M_PI);
  } else {
    return (2 * M_PI - epsilon_) + slopeLeft_ * (phasetime - (2 * M_PI - breakpoint_));
  }
}

void DoubleSineGenerator::setDcw(double dcw) {
  double corrDcw = kDcwCorrectCoef * dcw;
  breakpoint_ = M_PI * (1 - corrDcw);
  slopeLeft_ = 2 / (1 - corrDcw);
  slopeRight_ = 2 / (1 + corrDcw);
}

double DoubleSineGenerator::getPhase(double phasetime) {
  if (phasetime < breakpoint_) {
    return slopeLeft_ * phasetime;
  } else {
    return 2 * M_PI + slopeRight_ * (phasetime - breakpoint_);
  }
}

void SawPulseGenerator::setDcw(double dcw) {
  double corrDcw = kDcwCorrectCoef * dcw;
  breakpoint_ = M_PI * (1 - corrDcw);
  epsilon_ = kPhaseEpsilon * corrDcw;
  slopeLeft_ = (M_PI - epsilon_) / (M_PI * (1 - corrDcw));
  slopeRight_ = kPhaseEpsilon / (1 + corrDcw);
}

double SawPulseGenerator::getPhase(double phasetime) {
  if (phasetime < M_PI) {
    return phasetime;
  } else if (M_PI <= phasetime && phasetime < M_PI + breakpoint_) {
    return M_PI + slopeLeft_ * (phasetime - M_PI);
  } else {
    return (2 * M_PI - epsilon_) + slopeRight_ * (phasetime - breakpoint_);
  }
}

void AbstractResonanceGenerator::setDcw(double dcw) {
  highFreqPhaseCoef_ = 1 + kMaxFreqMult * dcw;
}

double AbstractResonanceGenerator::getPhase(double phasetime) {
  return phasetime;
}

double AbstractResonanceGenerator::generate(double phasetime) {
  return getEnvelope(phasetime) * (-cos(highFreqPhaseCoef_ * phasetime) + 1) - 1;
}

double ResonanceSawToothGenerator::getEnvelope(double phasetime) {
  return 1 - phasetime / (2 * M_PI - kPhaseEpsilon);
}

double ResonanceTriangleGenerator::getEnvelope(double phasetime) {
  if (phasetime < M_PI) {
    return kPhaseEpsilon + (1 - kPhaseEpsilon) / M_PI * phasetime;
  } else {
    return kPhaseEpsilon + 1 - (1 - kPhaseEpsilon) / M_PI * (phasetime - M_PI);
  }
}

double ResonanceTrapezoidGenerator::getEnvelope(double phasetime) {
  if (phasetime < M_PI) {
    return 1.0;
  } else {
    return 1 - (phasetime - M_PI) / (M_PI - kPhaseEpsilon);
  }
}

PD::PD()
    : waveform_(Waveform::kSawTooth),
      phasetime_(0.0),
      sampleRate_(kDefaultSampleRate),
      generator_(std::make_unique<SawToothGenerator>()) {
}

void PD::setWaveform(int8 waveformIndex) {
  waveform_ = static_cast<Waveform>(waveformIndex);
  switch (waveform_) {
    case Waveform::kSawTooth:
      generator_ = std::make_unique<SawToothGenerator>();
      break;
    case Waveform::kSquare:
      generator_ = std::make_unique<SquareGenerator>();
      break;
    case Waveform::kPulse:
      generator_ = std::make_unique<PulseGenerator>();
      break;
    case Waveform::kDoubleSine:
      generator_ = std::make_unique<DoubleSineGenerator>();
      break;
    case Waveform::kSawPulse:
      generator_ = std::make_unique<SawPulseGenerator>();
      break;
    case Waveform::kResonanceSawTooth:
      generator_ = std::make_unique<ResonanceSawToothGenerator>();
      break;
    case Waveform::kResonanceTriangle:
      generator_ = std::make_unique<ResonanceTriangleGenerator>();
      break;
    case Waveform::kResonanceTrapezoid:
      generator_ = std::make_unique<ResonanceTrapezoidGenerator>();
      break;
    default:  // never reached
      break;
  }
}

void PD::setSampleRate(double sampleRate) {
  sampleRate_ = sampleRate;
  dcoEg_.setSampleRate(sampleRate);
  dcwEg_.setSampleRate(sampleRate);
  dcaEg_.setSampleRate(sampleRate);
}

void PD::resetPhase() {
  phasetime_ = 0.0;
}

double PD::generate(double freq, bool& isDcaEnd) {
  phasetime_ += 2 * M_PI * freq * (1 + kDcoEgPitchDepth * dcoEg_.generate()) / sampleRate_;  // TODO: temp impl
  if (phasetime_ >= 2 * M_PI) {
    phasetime_ -= 2 * M_PI;
  }
  generator_->setDcw(dcwEg_.generate());
  return dcaEg_.generate(isDcaEnd) * generator_->generate(phasetime_);
}

void PD::setupEg() {
  dcoEg_.setup();
  dcwEg_.setup();
  dcaEg_.setup();
}

void PD::setEgRate(int32 paramId, int32 index, ParamValue rate) {
  if (kParamDcoEgRate0 <= paramId && paramId <= kParamDcoEgRate7) {
    dcoEg_.setRate(index, rate);
  } else if (kParamDcwEgRate0 <= paramId && paramId <= kParamDcwEgRate7) {
    dcwEg_.setRate(index, rate);
  } else if (kParamDcaEgRate0 <= paramId && paramId <= kParamDcaEgRate7) {
    dcaEg_.setRate(index, rate);
  }
}

void PD::setEgLevel(int32 paramId, int32 index, ParamValue level) {
  if (kParamDcoEgLevel0 <= paramId && paramId <= kParamDcoEgLevel6) {
    dcoEg_.setLevel(index, level);
  } else if (kParamDcwEgLevel0 <= paramId && paramId <= kParamDcwEgLevel6) {
    dcwEg_.setLevel(index, level);
  } else if (kParamDcaEgLevel0 <= paramId && paramId <= kParamDcaEgLevel6) {
    dcaEg_.setLevel(index, level);
  }
}

void PD::setEgSustainPoint(int32 paramId, int8 point) {
  if (paramId == kParamDcoEgSustainPoint) {
    dcoEg_.setSustainPoint(point);
  } else if (paramId == kParamDcwEgSustainPoint) {
    dcwEg_.setSustainPoint(point);
  } else if (paramId == kParamDcaEgSustainPoint) {
    dcaEg_.setSustainPoint(point);
  }
}

void PD::setEgEndPoint(int32 paramId, int8 point) {
  if (paramId == kParamDcoEgEndPoint) {
    dcoEg_.setEndPoint(point);
  } else if (paramId == kParamDcwEgEndPoint) {
    dcwEg_.setEndPoint(point);
  } else if (paramId == kParamDcaEgEndPoint) {
    dcaEg_.setEndPoint(point);
  }
}

void PD::restartEg() {
  dcoEg_.restart();
  dcwEg_.restart();
  dcaEg_.restart();
}

void PD::haltEg() {
  dcoEg_.halt();
  dcwEg_.halt();
  dcaEg_.halt();
}

}  // namespace Vst
}  // namespace Steinberg
