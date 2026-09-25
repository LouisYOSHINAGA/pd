#include "eg.h"

#include <algorithm>
#include <cmath>

#include "const.h"

// CZ-101 envelope model, derived from recordings of the 16 factory presets.
//
// Chip side: the envelope accumulator is updated once per chip sample
// (8.96 MHz / 256 = 35 kHz) by a step taken from a 3-bit mantissa / 4-bit
// exponent code, step = (8 + (n & 7)) << (n >> 3), and stops at the target.
// Targets are "level code << 18" for DCA/DCW and "semitones * 12 << 16" for
// DCO. The CPU maps the panel rate r (0..99) to n = round(1.25 * r).
//
// Level encodings (panel level l = 0..99):
//   DCA: code = l + 28 (l = 0 -> 0). The accumulator drives an exponential
//        volume table of ~0.495 dB per code (see kVolume), so a 99 -> 0
//        release covers 127 codes while 99 -> 75 covers only 24.
//   DCW: code = round(127 * l / 99); depth is linear in the code.
//   DCO: l < 64 -> l / 8 semitones, l >= 64 -> 2 * (l - 60) semitones; the
//        pitch glides linearly in semitones.

namespace Steinberg {
namespace Vst {

namespace {

constexpr double kChipTickRate = 8.96e6 / 256.0;
constexpr double kDcaDcwUnitScale = 1 << 18;     // chip units per level code
constexpr double kDcoUnitScale = 12.0 * (1 << 16);  // chip units per semitone
constexpr int kVolumeTableSize = 512;             // 9-bit index = code * 4
constexpr double kVolumeTableOctaves = 10.5;      // 0.495 dB per level code
constexpr double kDcwMaxDepth = 0.95;             // DCW output at code 127
constexpr int32 kMaxPanelValue = 99;

double kVolume[kVolumeTableSize];

// Exponential table with integer (floored) amplitudes, as on the chip: the
// flooring makes the last ~20 dB of a decay fall off faster than a pure
// exponential, which matches the recorded release tails.
struct VolumeInit {
  VolumeInit() {
    double full = std::exp2(kVolumeTableOctaves);
    for (int i = 0; i < kVolumeTableSize; i++) {
      kVolume[i] = std::floor(std::exp2(kVolumeTableOctaves * i / (kVolumeTableSize - 1))) / full;
    }
  }
} gVolumeInit;

int32 toPanelValue(ParamValue normalized) {
  int32 value = static_cast<int32>(normalized * kMaxPanelValue + 0.5);
  return std::clamp(value, 0, kMaxPanelValue);
}

// Panel rate (0..99) -> chip rate code: round(1.25 * rate), ties to even.
int32 rateCode(int32 rate) {
  int32 x = 5 * rate;
  int32 code = x >> 2;
  int32 rem = x & 3;
  if (rem > 2 || (rem == 2 && (code & 1))) {
    code++;
  }
  return code;
}

int32 chipStep(int32 code) {
  return (8 + (code & 7)) << (code >> 3);
}

}  // namespace

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
  rates_[index] = toPanelValue(rate);
}

void EG::setLevel(int32 index, ParamValue level) {
  levels_[index] = toPanelValue(level);
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

double EG::levelToTarget(int32 level) const {
  switch (egKind_) {
    case EgKind::kDco:
      return (level < 64) ? level / 8.0 : 2.0 * (level - 60);
    case EgKind::kDcw:
      return (127 * level + kMaxPanelValue / 2) / kMaxPanelValue;
    case EgKind::kDca:
      return (level == 0) ? 0.0 : level + 28.0;
    default:  // never reached
      return 0.0;
  }
}

double EG::rateToDLevel(int32 rate) const {
  double unitScale = (egKind_ == EgKind::kDco) ? kDcoUnitScale : kDcaDcwUnitScale;
  return chipStep(rateCode(rate)) * (kChipTickRate / kInternalSampleRate) / unitScale;
}

void EG::setup(EgKind egKind) {
  egKind_ = egKind;
  level_ = 0.0;
  enter(0);
}

void EG::enter(int8 step) {
  step_ = step;
  target_ = (step == endPoint_) ? 0.0 : levelToTarget(levels_[step]);  // end step always goes to 0
  double dLevel = rateToDLevel(rates_[step]);
  dLevel_ = (target_ < level_) ? -dLevel : dLevel;
}

void EG::restart() {
  if (sustainPoint_ < endPoint_) {
    enter(sustainPoint_ + 1);  // release: continue after the sustain step
  } else {
    enter(endPoint_);  // no sustain: jump straight to the end step
  }
}

void EG::proceed() {
  if (step_ == endPoint_) {
    halt();
  } else if (step_ == sustainPoint_) {
    dLevel_ = 0.0;
    step_ = kEgStepSustain;
  } else {
    enter(step_ + 1);
  }
}

void EG::update() {
  if (step_ == kEgStepSustain || step_ == kEgStepHalt) {
    return;
  }

  level_ += dLevel_;
  if ((dLevel_ >= 0.0 && level_ >= target_) || (dLevel_ < 0.0 && level_ <= target_)) {
    level_ = target_;
    proceed();
  }
}

void EG::halt() {
  dLevel_ = 0.0;
  step_ = kEgStepHalt;
}

double EG::output() const {
  switch (egKind_) {
    case EgKind::kDco:  // pitch offset in semitones
      return level_;
    case EgKind::kDcw:  // phase distortion depth 0..kDcwMaxDepth
      return level_ / 127.0 * kDcwMaxDepth;
    case EgKind::kDca: {  // amplitude 0..1
      if (level_ <= 0.0) {
        return 0.0;
      }
      int index = std::min(static_cast<int>(level_ * 4.0), kVolumeTableSize - 1);
      return kVolume[index];
    }
    default:  // never reached
      return 0.0;
  }
}

double EG::generate() {
  double value = output();
  update();
  return value;
}

double EG::generate(bool& isEgEnd) {
  double value = output();
  update();
  isEgEnd = step_ == kEgStepHalt;
  return value;
}

}  // namespace Vst
}  // namespace Steinberg
