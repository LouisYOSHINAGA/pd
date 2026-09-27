#pragma once

#include <array>

#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg {
namespace Vst {

// The three envelope generators of one PD line. The enumerator order matches
// the EG sub-block order of the line parameter layout in const.h.
enum class EgKind {
  kDco = 0,
  kDcw,
  kDca,
  kNumEgKinds
};

// Eight-step rate/level envelope generator, modeled after the CZ-101.
//
// The model follows the real machine's split: the CPU converts each panel
// value (rate/level 0..99) into a chip code, and the sound chip's envelope
// unit moves a linear accumulator toward the step target by a fixed amount
// per chip tick. All three EGs share the same rate law; they differ only in
// how a level is encoded and how the accumulator is turned into an output
// (see eg.cpp for the constants and how they were measured).
class EG {
 protected:
  static constexpr int8 kNumEgSteps = 8;
  static constexpr int8 kEgStepHalt = -1;
  static constexpr int8 kEgStepSustain = -2;
  static constexpr int8 kEgSustainOff = kNumEgSteps;
  static constexpr int8 kEgSustainPointOffset = -1;
  static constexpr int8 kEgEndPointOffset = 1;

  EgKind egKind_;
  std::array<int32, kNumEgSteps> rates_;       // panel values 0..99
  std::array<int32, kNumEgSteps - 1> levels_;  // panel values 0..99
  int8 sustainPoint_;  // step index that holds until note-off, kEgSustainOff if none
  int8 endPoint_;      // index of the last step (its target is always 0)
  int8 step_;          // running step index, or kEgStepHalt / kEgStepSustain
  bool released_;      // true after note-off
  int8 keyFollow_;     // key follow value 0..9
  double speedFactor_; // DCA key follow speed factor of the current note
  int32 levelOffset_;  // DCW key follow: level codes subtracted from the targets
  double level_;       // accumulator, in the output unit of the EG kind
  double dLevel_;      // accumulator change per internal tick (signed)
  double target_;

  virtual double levelToTarget(int32 level) const;
  virtual double rateToDLevel(int32 rate) const;
  virtual void enter(int8 step);
  virtual void proceed();
  virtual void update();
  virtual double output() const;

 public:
  EG();
  virtual void setRate(int32 index, ParamValue rate);
  virtual void setLevel(int32 index, ParamValue level);
  virtual void setSustainPoint(int8 point);
  virtual void setEndPoint(int8 point);
  virtual void setKeyFollow(int8 value);
  // Starts the envelope for a note-on; `note` is the MIDI note after the
  // octave range shift (it selects the key follow amount).
  virtual void setup(EgKind egKind, int32 note);
  virtual void restart();
  virtual void halt();
  virtual double generate();
  virtual double generate(bool& isEgEnd);
};

// Highest DCW output (in the unit of the DCW EG output) the CZ-101 allows at
// the oscillator frequency `freq` in Hz; it falls on high pitches regardless
// of the key follow and follows pitch changes such as the DCO envelope.
double dcwLimit(double freq);

}  // namespace Vst
}  // namespace Steinberg
