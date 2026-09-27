#include "eg.h"

#include <algorithm>
#include <cmath>

#include "const.h"

// CZ-101 envelope model, derived from recordings of the 16 factory presets
// and of DCA/DCW/DCO rate and level sweeps.
//
// Chip side: the envelope accumulator is updated once per chip sample
// (8.96 MHz / 256 = 35 kHz) by a step taken from a 3-bit mantissa / 4-bit
// exponent code, step = (8 + (n & 7)) << (n >> 3), and stops at the target.
// Targets are "level code << 18" for DCA/DCW and "semitones * 14 << 16" for
// DCO. The CPU maps the panel rate r (0..99) to n = 119 * r / 99 + 2 for
// DCA/DCW and to n = 127 * r / 99 for DCO (the sysex rate values of Casio's
// tables; DCO glides at rates 10..50 agree within 0.5%). After note-off n is
// limited to 104 (step 65536), so even a rate-99 DCA release takes ~14 ms
// from full level instead of ~3 ms.
//
// Level encodings (panel level l = 0..99):
//   DCA: code = l + 28 (l = 0 -> 0). The accumulator drives an exponential
//        volume table of 1/12 octave (~0.5 dB) per code (see kVolume), so a
//        99 -> 0 release covers 127 codes while 99 -> 75 covers only 24.
//   DCW: code = 127 * l / 99, truncated (Casio's sysex table); depth is
//        linear in the code.
//   DCO: l < 64 -> l / 8 semitones, l >= 64 -> 2 * (l - 60) semitones; the
//        pitch glides linearly in semitones.
//
// DCA key follow multiplies every step speed (attack, decay and release) by
// (12 + k) / 12, where k is an integer depending on the key follow value and
// the note (see kDcaKeyFollowK); the after-note-off limit applies to the
// multiplied speed.
//
// DCW key follow leaves the speed alone and subtracts a code depending on the
// key follow value and the note from every DCW target (see kDcwKeyFollowS;
// floor 0). Separately, the chip caps the DCW output on high pitches whatever
// the key follow is (see dcwLimit): the cap follows the oscillator frequency,
// DCO envelope included, while the key follow uses the note only.

namespace Steinberg {
namespace Vst {

namespace {

constexpr double kChipTickRate = 8.96e6 / 256.0;
constexpr double kDcaDcwUnitScale = 1 << 18;     // chip units per level code
constexpr double kDcoUnitScale = 14.0 * (1 << 16);  // chip units per semitone
constexpr int kVolumeTableSize = 512;             // 9-bit index = code * 4
constexpr int kVolumeStepsPerOctave = 48;         // 1/12 octave per level code
constexpr int kVolumeUnityIndex = 127 * 4;        // code 127 = full scale
constexpr double kVolumeFullScale = 1024.0;       // chip amplitude at full scale
constexpr int kVolumeSilentBelow = 24;            // code 6: output rounds to 0
constexpr double kDcwMaxDepth = 0.95;             // DCW output at code 127
constexpr int32 kMaxPanelValue = 99;
constexpr int32 kRateCodeOffset = 2;              // DCA/DCW, A4, key follow 0 (presets and a panel-set patch)
constexpr int32 kDcoRateCodeMax = 127;            // DCO rate code at panel rate 99
constexpr int32 kReleaseRateCodeMax = 104;        // fastest rate after note-off
constexpr int32 kKeyFollowLowestNote = 36;        // C2: the CZ-101 folds MIDI notes into C2..C7
constexpr int32 kKeyFollowHighestNote = 96;       // C7
constexpr int32 kKeyFollowNumNotes = kKeyFollowHighestNote - kKeyFollowLowestNote + 1;

// k of the DCA key follow speed factor (12 + k) / 12, per key follow value and
// note C2..C7, measured on a CZ-101 at C2..C7, A4, A5 and (key follow 9) every
// note of C4..C6 and A6, interpolated geometrically in between.
constexpr int16 kDcaKeyFollowK[kNumKeyFollowOptions][kKeyFollowNumNotes] = {
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  // key follow 0
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,
     1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3},  // key follow 1
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,
     1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,
     4,   4,   5,   5,   6,   6,   7,   8,   8,   9,  10,  11,  12},  // key follow 2
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,
     2,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,   7,   7,
     8,   9,   9,  10,  11,  12,  13,  14,  15,  16,  17,  19,  20},  // key follow 3
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
     3,   3,   3,   4,   4,   4,   5,   5,   6,   7,   7,   8,   9,  10,  11,  11,
    12,  13,  15,  16,  18,  20,  22,  24,  27,  30,  33,  36,  40},  // key follow 4
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,
     4,   4,   5,   5,   6,   7,   7,   8,   9,  10,  11,  12,  13,  14,  15,  17,
    18,  20,  23,  26,  29,  33,  37,  42,  47,  53,  60,  67,  76},  // key follow 5
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
     1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,
     5,   6,   7,   7,   8,   9,  10,  10,  11,  13,  14,  15,  16,  18,  20,  22,
    24,  27,  31,  36,  41,  46,  53,  60,  69,  78,  89, 102, 116},  // key follow 6
  {  0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
     1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,   6,
     7,   8,   9,   9,  10,  11,  12,  13,  15,  16,  18,  20,  22,  24,  28,  32,
    36,  42,  49,  56,  65,  76,  88, 101, 118, 136, 158, 183, 212},  // key follow 7
  {  0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
     1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   6,   7,   8,
     9,  10,  11,  13,  14,  16,  17,  19,  21,  24,  26,  29,  32,  36,  40,  44,
    48,  57,  68,  80,  95, 113, 134, 159, 188, 223, 264, 314, 372},  // key follow 8
  {  0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,
     2,   2,   2,   2,   3,   3,   3,   4,   4,   4,   6,   6,   8,   8,  10,  10,
    12,  12,  14,  16,  16,  18,  20,  20,  24,  28,  32,  36,  40,  44,  52,  60,
    69,  82,  98, 117, 140, 167, 199, 238, 285, 340, 438, 564, 727},  // key follow 9
};

// Level codes subtracted from the DCW targets, per key follow value and note
// C2..C7 (analysis/dcw_keyfollow_s.csv). Measured on a CZ-101 for every key
// follow value at C6 and C7 and for 3, 5, 7, 9 at C4..C7; the table is the fit
// s(key follow) * g(note), within 1.5 codes of every measured point.
constexpr int16 kDcwKeyFollowS[kNumKeyFollowOptions][kKeyFollowNumNotes] = {
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},  // key follow 0
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
     2,   2,   2,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   6,   6,
     6,   7,   7,   8,   8,   9,  10,  10,  11,  11,  12,  13,  14},  // key follow 1
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,
     3,   3,   3,   4,   4,   4,   5,   5,   5,   6,   6,   6,   7,   7,   8,   8,
     9,   9,  10,  11,  11,  12,  13,  14,  15,  15,  17,  18,  20},  // key follow 2
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   4,
     4,   4,   4,   5,   5,   6,   6,   6,   7,   7,   8,   8,   9,  10,  10,  11,
    11,  12,  13,  14,  15,  16,  17,  18,  19,  21,  22,  24,  26},  // key follow 3
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     1,   1,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,   4,   4,   5,
     5,   5,   6,   6,   7,   7,   8,   8,   9,  10,  10,  11,  12,  12,  13,  14,
    15,  16,  17,  18,  20,  21,  22,  24,  25,  27,  29,  31,  34},  // key follow 4
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     1,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   5,
     6,   6,   7,   7,   8,   9,   9,  10,  11,  11,  12,  13,  14,  15,  16,  17,
    18,  19,  20,  22,  23,  25,  26,  28,  30,  32,  34,  37,  40},  // key follow 5
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
     1,   1,   1,   2,   2,   2,   3,   3,   3,   4,   4,   4,   5,   5,   6,   6,
     7,   7,   8,   9,   9,  10,  11,  12,  12,  13,  14,  15,  16,  17,  18,  19,
    21,  22,  24,  25,  27,  29,  31,  33,  35,  37,  40,  43,  47},  // key follow 6
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
     1,   1,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,   6,   7,   7,
     8,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  23,
    24,  26,  28,  30,  32,  34,  36,  39,  41,  43,  47,  51,  55},  // key follow 7
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,
     1,   2,   2,   2,   3,   3,   4,   5,   5,   6,   6,   7,   8,   9,   9,  10,
    11,  11,  12,  13,  15,  16,  17,  18,  20,  21,  23,  24,  25,  27,  29,  31,
    33,  35,  37,  40,  43,  46,  49,  52,  55,  58,  63,  68,  74},  // key follow 8
  {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   2,
     2,   3,   4,   4,   5,   6,   7,   8,   9,  10,  11,  13,  14,  15,  17,  18,
    19,  20,  22,  24,  26,  28,  30,  33,  35,  38,  40,  43,  45,  48,  51,  55,
    58,  62,  67,  72,  77,  82,  87,  93,  98, 104, 113, 122, 127},  // key follow 9
};

// DCW cap: level code per oscillator frequency (Hz), measured with key follow
// 0 at C4..C7 (and at C6 raised to C7 by the DCO envelope). Linear in between;
// the lowest segment is extended up to code 127 and the highest one above C7.
constexpr double kDcwLimitFreq[] = {
  149.6, 261.6, 370.0, 523.3, 622.3, 740.0, 880.0, 1046.5, 1244.5, 1480.0, 1760.0, 2093.0};
constexpr double kDcwLimitCode[] = {
  127.0, 123.9, 120.9, 116.1, 113.1, 110.1, 106.2, 102.3, 98.2, 91.6, 82.0, 75.1};
constexpr int kNumDcwLimitPoints = sizeof(kDcwLimitFreq) / sizeof(kDcwLimitFreq[0]);

double kVolume[kVolumeTableSize];

// The chip amplitude is an integer, 1024 * 2^((code - 127) / 12), truncated
// when applied to the waveform. Truncation costs half an LSB on average,
// which makes the bottom ~20 dB of the range steeper than the 0.5 dB/code
// line; below code ~6 nothing is left. The table stores that averaged gain
// (it matches sustained levels within 0.15 dB and slow ramps within 0.5 dB).
struct VolumeInit {
  VolumeInit() {
    for (int i = 0; i < kVolumeTableSize; i++) {
      double amplitude = kVolumeFullScale *
          std::exp2(static_cast<double>(i - kVolumeUnityIndex) / kVolumeStepsPerOctave);
      kVolume[i] = (i < kVolumeSilentBelow) ? 0.0 : (amplitude - 0.5) / kVolumeFullScale;
    }
  }
} gVolumeInit;

int32 toPanelValue(ParamValue normalized) {
  int32 value = static_cast<int32>(normalized * kMaxPanelValue + 0.5);
  return std::clamp(value, 0, kMaxPanelValue);
}

// Panel rate (0..99) -> chip rate code (DCA/DCW 2..121, DCO 0..127).
int32 rateCode(EgKind egKind, int32 rate) {
  if (egKind == EgKind::kDco) {
    return kDcoRateCodeMax * rate / kMaxPanelValue;
  }
  return 119 * rate / kMaxPanelValue + kRateCodeOffset;
}

int32 chipStep(int32 code) {
  return (8 + (code & 7)) << (code >> 3);
}

int32 keyFollowIndex(int32 note) {
  return std::clamp(note, kKeyFollowLowestNote, kKeyFollowHighestNote) - kKeyFollowLowestNote;
}

double dcaKeyFollowFactor(int8 keyFollow, int32 note) {
  return (12.0 + kDcaKeyFollowK[keyFollow][keyFollowIndex(note)]) / 12.0;
}

}  // namespace

EG::EG()
    : egKind_(EgKind::kDco),  // dummy value for initialize
      rates_{},
      levels_{},
      sustainPoint_(kEgSustainOff),  // default: Off
      endPoint_(kEgEndPointOffset),  // default: 2
      step_(kEgStepHalt),
      released_(false),
      keyFollow_(0),
      speedFactor_(1.0),
      levelOffset_(0),
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

void EG::setKeyFollow(int8 value) {
  keyFollow_ = static_cast<int8>(std::clamp<int>(value, 0, kNumKeyFollowOptions - 1));
}

double EG::levelToTarget(int32 level) const {
  switch (egKind_) {
    case EgKind::kDco:
      return (level < 64) ? level / 8.0 : 2.0 * (level - 60);
    case EgKind::kDcw:
      return std::max(127 * level / kMaxPanelValue - levelOffset_, 0);
    case EgKind::kDca:
      return (level == 0) ? 0.0 : level + 28.0;
    default:  // never reached
      return 0.0;
  }
}

double EG::rateToDLevel(int32 rate) const {
  double step = chipStep(rateCode(egKind_, rate)) * speedFactor_;
  if (released_) {
    step = std::min(step, static_cast<double>(chipStep(kReleaseRateCodeMax)));
  }
  double unitScale = (egKind_ == EgKind::kDco) ? kDcoUnitScale : kDcaDcwUnitScale;
  return step * (kChipTickRate / kInternalSampleRate) / unitScale;
}

void EG::setup(EgKind egKind, int32 note) {
  egKind_ = egKind;
  speedFactor_ = (egKind == EgKind::kDca) ? dcaKeyFollowFactor(keyFollow_, note) : 1.0;
  levelOffset_ = (egKind == EgKind::kDcw) ? kDcwKeyFollowS[keyFollow_][keyFollowIndex(note)] : 0;
  released_ = false;
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
  if (released_) {
    return;  // only the first note-off counts
  }
  released_ = true;
  if (step_ == kEgStepSustain) {
    enter(sustainPoint_ + 1);  // release: continue after the sustain step
  } else {
    // Released before the sustain point was reached, or no sustain point:
    // the CZ-101 jumps straight to the end step.
    enter(endPoint_);
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

double dcwLimit(double freq) {
  int i = 1;
  while (i < kNumDcwLimitPoints - 1 && freq > kDcwLimitFreq[i]) {
    i++;
  }
  double code = kDcwLimitCode[i - 1] + (kDcwLimitCode[i] - kDcwLimitCode[i - 1]) *
      (freq - kDcwLimitFreq[i - 1]) / (kDcwLimitFreq[i] - kDcwLimitFreq[i - 1]);
  return std::clamp(code, 0.0, 127.0) / 127.0 * kDcwMaxDepth;
}

}  // namespace Vst
}  // namespace Steinberg
