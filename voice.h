#pragma once

#include <array>
#include <cstdint>
#include "pluginterfaces/vst/vsttypes.h"
#include "const.h"
#include "pd.h"

namespace Steinberg {
namespace Vst {

// Converts a MIDI note number to its equivalent frequency in Hz, A4 = 440 Hz.
double noteToFreq(int note);

// One polyphonic voice: a note assignment plus the PD sound sources needed to
// realize the CZ LINE SELECT modes. Three PD units are kept per voice:
//   unit 0: line 1
//   unit 1: detuned copy of line 1 (the 1' of mode 1+1'; mirrors line 1 params)
//   unit 2: line 2 (also serves as the detuned 2' of mode 1+2')
// PDProcessor owns a fixed-size pool of these and allocates them per note-on.
class Voice {
 public:
  Voice();

  // True once every sounding line's DCA envelope has fully halted and the
  // voice can be reused for a new note-on without stealing.
  bool isFree() const;

  // True if this voice is currently held down by the given channel/note,
  // i.e. it should respond to a matching note-off.
  bool isHeld(int channel, int note) const;

  // True while the voice is producing sound (attack through release tail).
  bool isActive() const;

  // Allocation order counter, used to find the oldest voice to steal.
  uint64_t age() const;

  void noteOn(int channel, int note, uint64_t age);
  void noteOff();
  double generate(double pitchBend);

  // Broadcast setters: these settings are shared across all voices.
  void setSampleRate(double sampleRate);
  void setLineSelect(LineSelect lineSelect);
  void setDetuneRatio(double ratio);
  // Applies one parameter of a line block; `line` is 0 (line 1) or 1 (line 2),
  // `offset` is the position within the block (see layout in const.h).
  // Line-1 parameters are mirrored into the detuned line-1 unit.
  void setLineParam(int32 line, int32 offset, ParamValue value);

 private:
  static constexpr int kNumUnits = 3;
  static constexpr int kUnitLine1 = 0;
  static constexpr int kUnitLine1Detuned = 1;
  static constexpr int kUnitLine2 = 2;

  // Sum of the two stacked lines is halved so that dual-line modes stay at
  // a per-note loudness comparable to the single-line modes.
  static constexpr double kDualLineMixGain = 0.5;

  static void applyLineParam(PD& pd, int32 offset, ParamValue value);

  // Runs one unit for one sample and accumulates into `out`; returns true
  // while the unit's DCA envelope is still running.
  bool runUnit(int unit, double freq, double& out);

  std::array<PD, kNumUnits> pds_;
  std::array<bool, kNumUnits> egEnded_;
  LineSelect lineSelect_;
  double detuneRatio_;
  int channel_;
  int note_;
  double baseFreq_;
  uint64_t age_;
  bool held_;
  bool active_;
};

}  // namespace Vst
}  // namespace Steinberg
