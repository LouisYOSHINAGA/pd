#include "vibrato.h"

namespace Steinberg {
namespace Vst {

namespace {

// One dial step of the shared vibrato curve. The step is 1 over the lower half
// of the dial and then doubles every 16 positions, which is what makes the
// upper half of DELAY / RATE / DEPTH stretch out so quickly on the hardware.
int32 vibratoDialStep(int32 dial) {
  return dial < 32 ? 1 : (1 << (1 + (dial - 32) / 16));
}

// The curve itself: the running sum of the steps up to `dial`. This reproduces
// the CZ sysex DELAY table exactly (0..31 then 33, 35, ... 639).
int32 vibratoDialUnits(int32 dial) {
  int32 units = 0;
  for (int32 i = 1; i <= dial; i++) {
    units += vibratoDialStep(i);
  }
  return units;
}

// RATE and DEPTH share a second curve, the same one offset by the current
// step. The sysex tables are reproduced by 32 * this for RATE and by this for
// DEPTH, except that DEPTH at the very top of the dial jumps to 768 instead of
// continuing the pattern (671) -- a deliberate extra push on the hardware,
// kept here so that the deepest setting matches.
constexpr int32 kVibratoDepthUnitsMax = 768;

int32 vibratoScaleUnits(int32 dial) {
  return vibratoDialUnits(dial) + vibratoDialStep(dial);
}

int32 vibratoDepthUnits(int32 dial) {
  return dial >= kVibratoDialMax ? kVibratoDepthUnitsMax : vibratoScaleUnits(dial);
}

int32 vibratoRateUnits(int32 dial) {
  return 32 * vibratoScaleUnits(dial);
}

// --- calibration -----------------------------------------------------------
// The modulation clock the hardware counts DELAY in and steps the LFO phase
// accumulator with. It puts the longest delay (639 units) at just under five
// seconds and the rate dial at roughly 0.06 Hz .. 42 Hz, which is the range
// the CZ's vibrato covers.
constexpr double kVibratoClockHz = 128.0;
// Width of the LFO phase accumulator the rate units are added to.
constexpr double kVibratoPhaseResolution = 65536.0;
// Pitch deviation of the deepest setting (depth 99). Two semitones either way
// puts a natural ~20 cent vibrato at mid-dial and leaves the top of the dial
// for the siren-like sweeps the CZ's deepest settings are known for.
constexpr double kVibratoMaxDepthSemitones = 2.0;

// Length of the ramp that eases the LFO in once the delay has elapsed. Not a
// hardware parameter: it only keeps the square wave from stepping the pitch
// discontinuously at the instant vibrato starts.
constexpr int32 kVibratoFadeInTicks = static_cast<int32>(0.05 * kInternalSampleRate);

int32 clampDial(int32 dial) {
  if (dial < 0) {
    return 0;
  }
  return dial > kVibratoDialMax ? kVibratoDialMax : dial;
}

}  // namespace

Vibrato::Vibrato()
    : wave_(VibratoWave::kTriangle),
      phase_(0.0),
      phaseStep_(0.0),
      depthSemitones_(0.0),
      delayTicks_(0),
      elapsedTicks_(0) {
  setRate(0);
  setDepth(0);
}

void Vibrato::setWave(VibratoWave wave) {
  wave_ = wave;
}

void Vibrato::setDelay(int dial) {
  delayTicks_ = static_cast<int32>(
    vibratoDialUnits(clampDial(dial)) * kInternalSampleRate / kVibratoClockHz
  );
}

void Vibrato::setRate(int dial) {
  double hz = vibratoRateUnits(clampDial(dial)) * kVibratoClockHz / kVibratoPhaseResolution;
  phaseStep_ = hz / kInternalSampleRate;
}

void Vibrato::setDepth(int dial) {
  depthSemitones_ = kVibratoMaxDepthSemitones
                  * vibratoDepthUnits(clampDial(dial)) / kVibratoDepthUnitsMax;
}

void Vibrato::trigger() {
  phase_ = 0.0;
  elapsedTicks_ = 0;
}

double Vibrato::shape() const {
  switch (wave_) {
    case VibratoWave::kTriangle:
      if (phase_ < 0.25) {
        return 4 * phase_;
      } else if (phase_ < 0.75) {
        return 2 - 4 * phase_;
      } else {
        return 4 * phase_ - 4;
      }
    case VibratoWave::kSawUp:
      // rises through zero to +1, drops to -1 and climbs back to zero
      return phase_ < 0.5 ? 2 * phase_ : 2 * phase_ - 2;
    case VibratoWave::kSawDown:
      return phase_ < 0.5 ? -2 * phase_ : 2 - 2 * phase_;
    case VibratoWave::kSquare:
      return phase_ < 0.5 ? 1.0 : -1.0;
    default:  // never reached
      return 0.0;
  }
}

double Vibrato::generate() {
  if (elapsedTicks_ < delayTicks_ + kVibratoFadeInTicks) {
    elapsedTicks_++;
  }

  double amount;
  if (elapsedTicks_ <= delayTicks_) {
    amount = 0.0;
  } else if (elapsedTicks_ >= delayTicks_ + kVibratoFadeInTicks) {
    amount = 1.0;
  } else {
    amount = static_cast<double>(elapsedTicks_ - delayTicks_) / kVibratoFadeInTicks;
  }

  double offset = amount * depthSemitones_ * shape();

  phase_ += phaseStep_;
  while (phase_ >= 1.0) {
    phase_ -= 1.0;
  }
  return offset;
}

}  // namespace Vst
}  // namespace Steinberg
