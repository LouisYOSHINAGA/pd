#pragma once

#include <array>
#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg {
namespace Vst {

// Eight-step rate/level envelope generator, modeled after the CZ series DCO/DCW/DCA EG.
class EG {
 protected:
  static constexpr int8 kNumEgSteps = 8;
  static constexpr int8 kEgStepHalt = -1;
  static constexpr int8 kEgStepSustain = -2;
  static constexpr int8 kEgSustainOff = kNumEgSteps;
  static constexpr int8 kEgSustainPointOffset = -1;
  static constexpr int8 kEgEndPointOffset = 1;

  std::array<double, kNumEgSteps> rates_;
  std::array<double, kNumEgSteps - 1> levels_;
  int8 sustainPoint_;
  int8 endPoint_;
  int8 step_;
  double level_;
  double dLevel_;
  double target_;
  double sampleRate_;

  virtual double rateToSample(double rate);
  virtual void proceed(int8 step);
  virtual void update();

 public:
  EG();
  virtual void setRate(int32 index, ParamValue rate);
  virtual void setLevel(int32 index, ParamValue level);
  virtual void setSustainPoint(int8 point);
  virtual void setEndPoint(int8 point);
  virtual void setSampleRate(double sampleRate);
  virtual void setup();
  virtual void restart();
  virtual void halt();
  virtual double generate();
  virtual double generate(bool& isEgEnd);
};

}  // namespace Vst
}  // namespace Steinberg
