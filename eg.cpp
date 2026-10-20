#include "eg.h"

#include <algorithm>
#include <cmath>

#include "const.h"

// CZ-101 envelope model, derived from recordings of the 16 factory presets
// and of DCA/DCW/DCO rate and level sweeps, and checked against the rate and
// level tables of a CZ book (chapter II "pro-data", pp. 96-99; see
// analysis/README.md).
//
// Chip side: the envelope accumulator is updated once per chip sample
// (8.96 MHz / 256 = 35 kHz) by a step taken from a 3-bit mantissa / 4-bit
// exponent code n, step = (8 + (n & 7)) << (n >> 3), plus half a mantissa LSB
// (1 << ((n >> 3) - 1)) from exponent 9 (n >= 72) up, and stops at the
// target. Targets are "level code * 14 << 14" for DCA, "level code * 14 << 15"
// for DCW and "semitones * 14 << 16" for DCO. The CPU maps the panel rate r
// (0..99) to n = 119 * r / 99 for DCA, 119 * r / 99 + 8 for DCW (twice the
// step on twice the scale, so DCW moves as many level codes per second as
// DCA) and 127 * r / 99 for DCO (the sysex rate values of Casio's tables).
// This reproduces the book's rate table (one misprint aside), and the recordings
// agree: DCO glides at rates 10..60 within 1%, DCA attacks at rates 24 and 36
// (key follow 0) within 0.1 code. After note-off n is limited to 102 (DCW
// 110, the same speed; DCO assumed 102), so even a rate-99 DCA release takes
// ~14 ms from full level instead of ~3 ms.
//
// Level encodings (panel level l = 0..99):
//   DCA: code = l + 28 (l = 0 -> 0). The accumulator drives an exponential
//        volume table of 1/12 octave (~0.5 dB) per code (see kVolume), so a
//        99 -> 0 release covers 127 codes while 99 -> 75 covers only 24.
//   DCW: code = 127 * l / 99, truncated (Casio's sysex table); the EG outputs
//        code / 127 and the phase distortion depth is linear in it (0.97 at
//        code 127, see PD's generators).
//   DCO: l < 64 -> l / 8 semitones, l >= 64 -> 2 * (l - 60) semitones; the
//        pitch glides linearly in semitones.
//
// DCA key follow adds a code depending on the key follow value and the note to
// the rate code of every step (attack, decay and release; see
// kDcaKeyFollowCode); the after-note-off limit applies to the raised code.
// (At rate 24, code 28, this looks like a speed factor (12 + k) / 12 with an
// integer k, but at rate 36 the speeds follow the code.)
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
constexpr double kDcaUnitScale = 14.0 * (1 << 14);  // chip units per level code
constexpr double kDcwUnitScale = 14.0 * (1 << 15);  // chip units per level code
constexpr double kDcoUnitScale = 14.0 * (1 << 16);  // chip units per semitone
constexpr int kVolumeTableSize = 512;             // 9-bit index = code * 4
constexpr int kVolumeStepsPerOctave = 48;         // 1/12 octave per level code
constexpr int kVolumeUnityIndex = 127 * 4;        // code 127 = full scale
constexpr double kVolumeFullScale = 1024.0;       // chip amplitude at full scale
constexpr int kVolumeSilentBelow = 24;            // code 6: output rounds to 0
constexpr int32 kMaxPanelValue = 99;
constexpr int32 kDcaRateCodeMax = 119;            // DCA rate code at panel rate 99
constexpr int32 kDcwRateCodeOffset = 8;           // DCW rate code = DCA rate code + 8
constexpr int32 kDcoRateCodeMax = 127;            // DCO rate code at panel rate 99
constexpr int32 kMaxRateCode = 127;               // 7-bit chip rate code (key follow can exceed 119)
constexpr int32 kStepHalfLsbExponent = 9;         // the step gains half an LSB from here up
constexpr int32 kReleaseRateCodeMax = 102;        // fastest DCA/DCO rate after note-off
constexpr int32 kKeyFollowLowestNote = 36;        // C2: the CZ-101 folds MIDI notes into C2..C7
constexpr int32 kKeyFollowHighestNote = 96;       // C7
constexpr int32 kKeyFollowNumNotes = kKeyFollowHighestNote - kKeyFollowLowestNote + 1;

// Rate codes added to every DCA step by the key follow, per key follow value
// and note C2..C7 (analysis/dca_keyfollow_code.csv). Measured on a CZ-101 at
// C2..C7, A4, A5 and (key follow 9) every note of C4..C6 and A6, where every
// point is an integer within 0.07 code; interpolated linearly in between.
constexpr int16 kDcaKeyFollowCode[kNumKeyFollowOptions][kKeyFollowNumNotes] = {
  { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},  // key follow 0
  { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  2,  2,  2,  2,  2,  3,  3,  3,  3},  // key follow 1
  { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,  4,
    4,  4,  5,  5,  5,  6,  6,  6,  7,  7,  7,  8,  8},  // key follow 2
  { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  5,  6,
    6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12},  // key follow 3
  { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
    3,  3,  3,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,  7,  7,  8,
    8,  9, 10, 10, 11, 12, 13, 13, 14, 15, 16, 16, 17},  // key follow 4
  { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  4,
    4,  4,  4,  5,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10,
   11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},  // key follow 5
  { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,
    5,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 12, 12,
   13, 14, 15, 17, 18, 19, 20, 22, 23, 24, 25, 27, 28},  // key follow 6
  { 0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  4,  4,  4,  5,  5,  5,
    6,  6,  6,  7,  7,  8,  8,  9, 10, 10, 11, 12, 12, 13, 14, 15,
   16, 18, 19, 21, 22, 24, 25, 27, 28, 30, 31, 33, 34},  // key follow 7
  { 0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  4,  4,  5,  5,  6,  6,
    7,  7,  8,  8,  9, 10, 11, 11, 12, 13, 14, 14, 15, 16, 17, 18,
   19, 21, 23, 24, 26, 28, 30, 31, 33, 35, 37, 38, 40},  // key follow 8
  { 0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  3,  3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  7,
    8,  8,  9, 10, 10, 11, 12, 12, 13, 14, 15, 16, 17, 18, 20, 21,
   22, 24, 26, 28, 30, 31, 33, 35, 37, 39, 42, 44, 47},  // key follow 9
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

// Panel rate (0..99) -> chip rate code (DCA 0..119, DCW 8..127, DCO 0..127).
int32 rateCode(EgKind egKind, int32 rate) {
  switch (egKind) {
    case EgKind::kDco:
      return kDcoRateCodeMax * rate / kMaxPanelValue;
    case EgKind::kDcw:
      return kDcaRateCodeMax * rate / kMaxPanelValue + kDcwRateCodeOffset;
    case EgKind::kDca:
      return kDcaRateCodeMax * rate / kMaxPanelValue;
    default:  // never reached
      return 0;
  }
}

int32 releaseRateCodeMax(EgKind egKind) {
  return (egKind == EgKind::kDcw) ? kReleaseRateCodeMax + kDcwRateCodeOffset : kReleaseRateCodeMax;
}

int32 chipStep(int32 code) {
  int32 exponent = code >> 3;
  int32 step = (8 + (code & 7)) << exponent;
  if (exponent >= kStepHalfLsbExponent) {
    step += 1 << (exponent - 1);
  }
  return step;
}

double unitScale(EgKind egKind) {
  switch (egKind) {
    case EgKind::kDco:
      return kDcoUnitScale;
    case EgKind::kDcw:
      return kDcwUnitScale;
    default:
      return kDcaUnitScale;
  }
}

int32 keyFollowIndex(int32 note) {
  return std::clamp(note, kKeyFollowLowestNote, kKeyFollowHighestNote) - kKeyFollowLowestNote;
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
      rateCodeOffset_(0),
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
  int32 code = std::min(rateCode(egKind_, rate) + rateCodeOffset_, kMaxRateCode);
  if (released_) {
    code = std::min(code, releaseRateCodeMax(egKind_));
  }
  return chipStep(code) * (kChipTickRate / kInternalSampleRate) / unitScale(egKind_);
}

void EG::setup(EgKind egKind, int32 note) {
  egKind_ = egKind;
  rateCodeOffset_ = (egKind == EgKind::kDca) ? kDcaKeyFollowCode[keyFollow_][keyFollowIndex(note)] : 0;
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
  // The sustain/end points may have changed while the note was held (e.g. a
  // preset change); the step after the sustain step must not pass the end.
  if (step_ == kEgStepSustain && sustainPoint_ < endPoint_) {
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
  } else if (step_ > endPoint_) {
    enter(endPoint_);  // the end point was moved below the running step
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
    case EgKind::kDcw:  // DCW 0..1 (level code / 127)
      return level_ / 127.0;
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
  return std::clamp(code, 0.0, 127.0) / 127.0;
}

}  // namespace Vst
}  // namespace Steinberg
