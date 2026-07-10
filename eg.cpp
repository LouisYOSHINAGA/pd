#include "eg.h"

#include "const.h"

namespace Steinberg {
namespace Vst {

EG::EG()
    : rates_{},
      levels_{},
      sustainPoint_(kEgSustainOff),  // default: Off
      endPoint_(kEgEndPointOffset),  // default: 2
      step_(kEgStepHalt),
      level_(0.0),
      dLevel_(0.0),
      target_(0.0),
      sampleRate_(kDefaultSampleRate) {
}

void EG::setRate(int32 index, ParamValue rate) {
  rates_[index] = rate;
}

void EG::setLevel(int32 index, ParamValue level) {
  levels_[index] = level;
}

void EG::setSustainPoint(int8 point) {
  if (point == 0) {  // Off
    sustainPoint_ = kEgSustainOff;
  } else {
    sustainPoint_ = kEgSustainPointOffset + point;
  }
}

void EG::setEndPoint(int8 point) {
  endPoint_ = kEgEndPointOffset + point;
}

void EG::setSampleRate(double sampleRate) {
  sampleRate_ = sampleRate;
}

double EG::rateToSample(double rate) {
  return 5 * (1.0 - rate) * sampleRate_ + 100;  // TODO: temp impl
}

void EG::setup() {
  level_ = 0;
  target_ = levels_[0];
  dLevel_ = target_ / rateToSample(rates_[0]);
  step_ = 0;
}

void EG::restart() {
  if (endPoint_ <= sustainPoint_) {  // sustain off
    step_ = endPoint_;  // go to last step directly
  } else {
    step_ = sustainPoint_ + 1;
  }

  if (step_ == endPoint_) {
    target_ = 0;
  } else {
    target_ = levels_[step_];
  }
  dLevel_ = (target_ - level_) / rateToSample(rates_[step_]);
}

void EG::update() {
  if (step_ == kEgStepSustain || step_ == kEgStepHalt) {
    return;
  }

  level_ += dLevel_;
  if (step_ == endPoint_) {
    if ((dLevel_ >= 0 && level_ >= target_) || (dLevel_ <= 0 && level_ <= target_)) {
      halt();
    }
  } else if ((dLevel_ > 0 && level_ >= target_) || (dLevel_ < 0 && level_ <= target_)) {
    level_ = levels_[step_];
    proceed(step_);
  }
}

void EG::halt() {
  level_ = target_;
  dLevel_ = 0;
  step_ = kEgStepHalt;
}

void EG::proceed(int8 step) {
  if (step == sustainPoint_) {
    dLevel_ = 0;
    step_ = kEgStepSustain;
    return;
  }

  if (step == endPoint_ - 1) {
    target_ = 0;  // target level at end point must be 0
  } else {
    target_ = levels_[step + 1];
  }
  dLevel_ = (target_ - level_) / rateToSample(rates_[step + 1]);
  step_ = step + 1;
}

double EG::generate() {
  double level = level_;
  update();
  return level;
}

double EG::generate(bool& isEgEnd) {
  double level = level_;
  update();
  isEgEnd = step_ == kEgStepHalt;
  return level;
}

}  // namespace Vst
}  // namespace Steinberg
