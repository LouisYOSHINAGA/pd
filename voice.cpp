#include "voice.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace Steinberg {
namespace Vst {

double noteToFreq(int note) {
  return kA4Freq * pow(2.0, (note - kA4Note) / 12.0);
}

Voice::Voice()
    : egEnded_{},
      lineSelect_(LineSelect::kLine1),
      modulation_(Modulation::kOff),
      detuneRatio_(1.0),
      channel_(-1),
      note_(-1),
      baseFreq_(0.0),
      age_(0),
      held_(false),
      active_(false) {
}

bool Voice::isFree() const {
  return !active_;
}

bool Voice::isHeld(int channel, int note) const {
  return held_ && channel_ == channel && note_ == note;
}

bool Voice::isActive() const {
  return active_;
}

uint64_t Voice::age() const {
  return age_;
}

void Voice::noteOn(int channel, int note, uint64_t age) {
  channel_ = channel;
  note_ = note;
  baseFreq_ = noteToFreq(note);
  age_ = age;
  held_ = true;
  active_ = true;
  for (PD& pd : pds_) {
    pd.resetPhase();
    pd.setupEg();
  }
  egEnded_.fill(false);
}

void Voice::noteOff() {
  held_ = false;
  for (PD& pd : pds_) {
    pd.restartEg();
  }
}

bool Voice::runUnit(int unit, double freq, double& out) {
  if (egEnded_[unit]) {
    return false;
  }
  bool isDcaEnd = false;
  out += pds_[unit].generate(freq, isDcaEnd);
  if (isDcaEnd) {
    pds_[unit].haltEg();
    egEnded_[unit] = true;
    return false;
  }
  return true;
}

bool Voice::runDualLine(int second, double freq, double detunedFreq, double& out) {
  if (modulation_ != Modulation::kRing) {
    bool anyAlive = false;
    anyAlive |= runUnit(kUnitLine1, freq, out);
    anyAlive |= runUnit(second, detunedFreq, out);
    out *= kDualLineMixGain;
    return anyAlive;
  }

  // RING MODULATION: the two lines are multiplied rather than summed, which
  // replaces their harmonics with the sum and difference of the two -- the
  // inharmonic, bell-like spectrum the CZ's RING button is used for. Neither
  // line is audible on its own, so the product falls silent as soon as
  // *either* DCA envelope ends and the voice can be reused then.
  double first = 0.0;
  double primed = 0.0;
  bool firstAlive = runUnit(kUnitLine1, freq, first);
  bool primedAlive = runUnit(second, detunedFreq, primed);
  // Both factors are already amplitude-limited to [-1, 1] by their DCA, so
  // the product needs no headroom correction of its own.
  out += first * primed;
  return firstAlive && primedAlive;
}

double Voice::generate(double pitchOffset) {
  double freq = baseFreq_ * pow(2.0, pitchOffset / 12.0);
  double detunedFreq = freq * detuneRatio_;
  double out = 0.0;
  bool anyAlive = false;

  switch (lineSelect_) {
    case LineSelect::kLine1:
      anyAlive = runUnit(kUnitLine1, freq, out);
      break;
    case LineSelect::kLine2:
      anyAlive = runUnit(kUnitLine2, freq, out);
      break;
    case LineSelect::kLine1Plus1Detuned:
      anyAlive = runDualLine(kUnitLine1Detuned, freq, detunedFreq, out);
      break;
    case LineSelect::kLine1Plus2Detuned:
      anyAlive = runDualLine(kUnitLine2, freq, detunedFreq, out);
      break;
    default:  // never reached
      break;
  }

  if (!anyAlive) {
    active_ = false;
  }
  return out;
}

void Voice::setLineSelect(LineSelect lineSelect) {
  lineSelect_ = lineSelect;
}

void Voice::setModulation(Modulation modulation) {
  modulation_ = modulation;
}

void Voice::setDetuneRatio(double ratio) {
  detuneRatio_ = ratio;
}

void Voice::setLineParam(int32 line, int32 offset, ParamValue value) {
  if (line == 0) {
    applyLineParam(pds_[kUnitLine1], offset, value);
    applyLineParam(pds_[kUnitLine1Detuned], offset, value);
  } else {
    applyLineParam(pds_[kUnitLine2], offset, value);
  }
}

void Voice::applyLineParam(PD& pd, int32 offset, ParamValue value) {
  if (offset == kLineParamWaveformFirst) {
    pd.setWaveformFirst(static_cast<int8>(
      decodeOptionIndex(value, static_cast<int>(Waveform::kNumWaveforms))
    ));
    return;
  }
  if (offset == kLineParamWaveformSecond) {
    pd.setWaveformSecond(static_cast<int8>(
      decodeOptionIndex(value, kNumSecondWaveformOptions)
    ));
    return;
  }

  int32 egOffset = offset - kLineParamEgBegin;
  EgKind kind = static_cast<EgKind>(egOffset / kLineParamEgBlockSize);
  int32 index = egOffset % kLineParamEgBlockSize;
  if (index < kEgParamLevel0) {
    pd.setEgRate(kind, index - kEgParamRate0, value);
  } else if (index < kEgParamSustainPoint) {
    pd.setEgLevel(kind, index - kEgParamLevel0, value);
  } else if (index == kEgParamSustainPoint) {
    pd.setEgSustainPoint(kind, static_cast<int8>(
      decodeOptionIndex(value, kNumEgSustainPointOptions)
    ));
  } else {
    pd.setEgEndPoint(kind, static_cast<int8>(
      decodeOptionIndex(value, kNumEgEndPointOptions)
    ));
  }
}

}  // namespace Vst
}  // namespace Steinberg
