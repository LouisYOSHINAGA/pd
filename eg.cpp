#include "eg.h"

#include <cmath>

namespace {

double kVolume[512];

struct VolumeInit {
  VolumeInit() {
    kVolume[0] = 0.0;
    for (int i = 1; i < 512; i++){
      kVolume[i] = std::floor(std::pow(2.0, 13.0 * i / 511.0)) / 8192.0;
    }
  }
} gVolumeInit;

}  // namspace

namespace Steinberg {
namespace Vst {

EG::EG()
    : egKind_(EgKind::kDco),  // dummy value for initialize
      rates_{},
      levels_{},
      sustainPoint_(kEgSustainOff),  // default: Off
      endPoint_(kEgEndPointOffset),  // default: 2
      step_(kEgStepHalt),
      level_(0.0),
      dLevel_(0.0),
      target_(0.0) {
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

int32 EG::rateToDLevel(double rate) {
  uint8 rate7bit = (uint8)(127 * rate + 0.5) & 0x7F;
  return (8 + (rate7bit & 0x07)) << (rate7bit >> 3);
}

int EG::levelsToSign(double current, double target){
  return (current < target)? 1 : -1;
}

int32 EG::levelToTarget(double level) const {
  int32 target = (int32)(127 * level + 0.5) & 0x7F;

  if(egKind_ == EgKind::kDco){
    if(0x40 <= target && target <= 0x43){
      target = 0x3F;
    }
    if(target & 0x40){
      target = (target & 0x3F) << 5;
    }
  }

  return target << kEgBitData[static_cast<int>(egKind_)].shiftUpBit_;
}

void EG::setup(EgKind egKind) {
  egKind_ = egKind;
  level_ = 0;
  target_ = levelToTarget(levels_[0]);
  dLevel_ = levelsToSign(level_, target_) * rateToDLevel(rates_[0]);
  step_ = 0;
}

void EG::restart() {
  if (endPoint_ <= sustainPoint_) {  // sustain off
    step_ = endPoint_;  // go to last step directly
  } else {
    step_ = sustainPoint_ + 1;
  }

  target_ = levelToTarget(levels_[step_]);
  if (step_ == endPoint_) {
    target_ = 0;
  }
  dLevel_ = levelsToSign(level_, target_) * rateToDLevel(rates_[step_]);
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
    level_ = target_;
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

  target_ = levelToTarget(levels_[step_ + 1]);
  if (step == endPoint_ - 1) {
    target_ = 0;  // target level at end point must be 0
  }
  dLevel_ = levelsToSign(level_, target_) * rateToDLevel(rates_[step + 1]);
  step_ = step + 1;
}

int32 EG::levelToIndex() const {
  int32 index = level_ >> kEgBitData[static_cast<int>(egKind_)].shiftDownBit_;
  if(index < 0){
    index = 0;
  }
  return index;
}

double EG::generate() {
  double level;
  if (egKind_ == EgKind::kDca) {
    level = kVolume[levelToIndex()];
  }else{
    level = levelToIndex() / kEgBitData[static_cast<int>(egKind_)].outReso_;
  }
  update();
  return level;
}

double EG::generate(bool& isEgEnd) {
  double level;
  if (egKind_ == EgKind::kDca) {
    level = kVolume[levelToIndex()];
  }else{
    level = levelToIndex() / kEgBitData[static_cast<int>(egKind_)].outReso_;
  }
  update();
  isEgEnd = step_ == kEgStepHalt;
  return level;
}

}  // namespace Vst
}  // namespace Steinberg
