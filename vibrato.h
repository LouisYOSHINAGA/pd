#pragma once

#include "pluginterfaces/vst/vsttypes.h"

#include "const.h"

namespace Steinberg {
namespace Vst {

// The CZ VIBRATO section: a single global LFO that modulates the pitch of
// every sounding line, with the panel's four parameters WAVE / DELAY / RATE /
// DEPTH. One instance lives in PDProcessor and is advanced once per internal
// tick, exactly like the hardware's one vibrato generator shared by all voices.
//
// The 0..99 dials are far from linear. The CZ MIDI system-exclusive
// specification tabulates the internal value each dial position transmits, and
// all three tables turn out to be the same curve (see vibratoDialUnits):
// DELAY sends it directly, RATE sends 32x it, DEPTH sends it offset by one
// step. Those tables are reproduced exactly; what is *not* documented anywhere
// is the physical meaning of the units, so the three calibration constants
// below convert them to Hz / seconds / cents and are the only place to adjust
// if measurements of a real CZ say otherwise.
class Vibrato {
 public:
  Vibrato();

  virtual void setWave(VibratoWave wave);
  virtual void setDelay(int dial);  // 0..99
  virtual void setRate(int dial);   // 0..99
  virtual void setDepth(int dial);  // 0..99

  // Restarts the delay and the LFO phase. Called on the note-on that starts a
  // phrase, i.e. the first key pressed while no other key is held: the CZ has
  // one vibrato generator, so its delay tracks the phrase, not each note.
  virtual void trigger();

  // Advances the LFO by one internal tick and returns the pitch offset to add
  // to every voice, in semitones.
  virtual double generate();

 private:
  // Value of the LFO shape at the current phase, in [-1, 1]. All four shapes
  // are bipolar and start at 0 (or, for the square, at its positive half), so
  // that switching wave does not shift the perceived pitch of a note.
  double shape() const;

  VibratoWave wave_;
  double phase_;      // 0..1
  double phaseStep_;  // phase advance per internal tick
  double depthSemitones_;
  int32 delayTicks_;
  int32 elapsedTicks_;
};

}  // namespace Vst
}  // namespace Steinberg
