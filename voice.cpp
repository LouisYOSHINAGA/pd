#include "voice.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include "const.h"

namespace Steinberg {
namespace Vst {

double noteToFreq(int note) {
  return kA4Freq * pow(2.0, (note - kA4Note) / 12.0);
}

Voice::Voice()
    : channel_(-1), note_(-1), baseFreq_(0.0), age_(0), held_(false), active_(false) {
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
  pd_.resetPhase();
  pd_.setupEg();
}

void Voice::noteOff() {
  held_ = false;
  pd_.restartEg();
}

double Voice::generate(double pitchBend) {
  double freq = baseFreq_ * pow(2.0, pitchBend / 12.0);
  bool isDcaEnd = false;
  double sample = pd_.generate(freq, isDcaEnd);
  if (isDcaEnd) {
    pd_.haltEg();
    active_ = false;
  }
  return sample;
}

void Voice::setWaveform(int8 waveformIndex) {
  pd_.setWaveform(waveformIndex);
}

void Voice::setSampleRate(double sampleRate) {
  pd_.setSampleRate(sampleRate);
}

void Voice::setEgRate(int32 paramId, int32 index, ParamValue rate) {
  pd_.setEgRate(paramId, index, rate);
}

void Voice::setEgLevel(int32 paramId, int32 index, ParamValue level) {
  pd_.setEgLevel(paramId, index, level);
}

void Voice::setEgSustainPoint(int32 paramId, int8 point) {
  pd_.setEgSustainPoint(paramId, point);
}

void Voice::setEgEndPoint(int32 paramId, int8 point) {
  pd_.setEgEndPoint(paramId, point);
}

}  // namespace Vst
}  // namespace Steinberg
