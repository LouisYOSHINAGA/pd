#pragma once

#include <cstdint>
#include "pluginterfaces/vst/vsttypes.h"
#include "pd.h"

namespace Steinberg {
namespace Vst {

// Converts a MIDI note number to its equivalent frequency in Hz, A4 = 440 Hz.
double noteToFreq(int note);

// One polyphonic voice: a note assignment plus its own PD sound source.
// PDProcessor owns a fixed-size pool of these and allocates them per note-on.
class Voice {
 public:
  Voice();

  // True once the voice's DCA envelope has fully halted and it can be
  // reused for a new note-on without stealing.
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

  // Broadcast setters: waveform and EG tables are shared across all voices.
  void setWaveform(int8 waveformIndex);
  void setSampleRate(double sampleRate);
  void setEgRate(int32 paramId, int32 index, ParamValue rate);
  void setEgLevel(int32 paramId, int32 index, ParamValue level);
  void setEgSustainPoint(int32 paramId, int8 point);
  void setEgEndPoint(int32 paramId, int8 point);

 private:
  PD pd_;
  int channel_;
  int note_;
  double baseFreq_;
  uint64_t age_;
  bool held_;
  bool active_;
};

}  // namespace Vst
}  // namespace Steinberg
