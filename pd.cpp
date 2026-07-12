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

namespace {

std::unique_ptr<AbstractGenerator> makeGenerator(int8 waveformIndex) {
  switch (static_cast<Waveform>(waveformIndex)) {
    case Waveform::kSawTooth:
      return std::make_unique<SawToothGenerator>();
    case Waveform::kSquare:
      return std::make_unique<SquareGenerator>();
    case Waveform::kPulse:
      return std::make_unique<PulseGenerator>();
    case Waveform::kDoubleSine:
      return std::make_unique<DoubleSineGenerator>();
    case Waveform::kSawPulse:
      return std::make_unique<SawPulseGenerator>();
    case Waveform::kResonanceSawTooth:
      return std::make_unique<ResonanceSawToothGenerator>();
    case Waveform::kResonanceTriangle:
      return std::make_unique<ResonanceTriangleGenerator>();
    case Waveform::kResonanceTrapezoid:
      return std::make_unique<ResonanceTrapezoidGenerator>();
    default:  // never reached
      return std::make_unique<SawToothGenerator>();
  }
}

}  // namespace

PD::PD()
    : phasetime_(0.0),
      sampleRate_(kDefaultSampleRate),
      onSecondWaveform_(false),
      generatorFirst_(std::make_unique<SawToothGenerator>()),
      generatorSecond_(nullptr) {
}

void PD::setWaveformFirst(int8 waveformIndex) {
  generatorFirst_ = makeGenerator(waveformIndex);
}

void PD::setWaveformSecond(int8 selection) {
  if (selection == 0) {  // Off
    generatorSecond_.reset();
    onSecondWaveform_ = false;
  } else {
    generatorSecond_ = makeGenerator(selection - 1);
  }
}

void PD::setSampleRate(double sampleRate) {
  sampleRate_ = sampleRate;
  for (EG& eg : egs_) {
    eg.setSampleRate(sampleRate);
  }
}

void PD::resetPhase() {
  phasetime_ = 0.0;
  onSecondWaveform_ = false;
}

double PD::generate(double freq, bool& isDcaEnd) {
  phasetime_ += 2 * M_PI * freq * (1 + kDcoEgPitchDepth * eg(EgKind::kDco).generate()) / sampleRate_;  // TODO: temp impl
  if (phasetime_ >= 2 * M_PI) {
    phasetime_ -= 2 * M_PI;
    // When a second waveform is selected, alternate waveforms every cycle
    // (CZ behavior); the combined waveform then repeats every two cycles.
    if (generatorSecond_ != nullptr) {
      onSecondWaveform_ = !onSecondWaveform_;
    }
  }
  AbstractGenerator* generator =
      onSecondWaveform_ ? generatorSecond_.get() : generatorFirst_.get();
  generator->setDcw(eg(EgKind::kDcw).generate());
  return eg(EgKind::kDca).generate(isDcaEnd) * generator->generate(phasetime_);
}

void PD::setupEg() {
  for (EG& eg : egs_) {
    eg.setup();
  }
}

void PD::setEgRate(EgKind kind, int32 index, ParamValue rate) {
  eg(kind).setRate(index, rate);
}

void PD::setEgLevel(EgKind kind, int32 index, ParamValue level) {
  eg(kind).setLevel(index, level);
}

void PD::setEgSustainPoint(EgKind kind, int8 point) {
  eg(kind).setSustainPoint(point);
}

void PD::setEgEndPoint(EgKind kind, int8 point) {
  eg(kind).setEndPoint(point);
}

void PD::restartEg() {
  for (EG& eg : egs_) {
    eg.restart();
  }
}

void PD::haltEg() {
  for (EG& eg : egs_) {
    eg.halt();
  }
}

}  // namespace Vst
}  // namespace Steinberg
