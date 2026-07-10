#pragma once

#include <memory>
#include "pluginterfaces/vst/vsttypes.h"
#include "const.h"
#include "eg.h"

namespace Steinberg {
namespace Vst {

class AbstractGenerator {
 protected:
  static constexpr double kDcwCorrectCoef = 0.95;
  double breakpoint_;
  double slopeLeft_;
  double slopeRight_;
  // DCW-scaled transition width used by generators with a fast breakpoint
  // segment (Square/Pulse/SawPulse), so that it vanishes as dcw -> 0 and
  // the waveform converges to a pure cosine instead of a biased one.
  double epsilon_;

 public:
  AbstractGenerator();
  virtual ~AbstractGenerator() = default;
  virtual void setDcw(double dcw) = 0;
  virtual double getPhase(double phasetime) = 0;
  virtual double generate(double phasetime);
};

class SawToothGenerator : public AbstractGenerator {
 public:
  SawToothGenerator() = default;
  void setDcw(double dcw) override;
  double getPhase(double phasetime) override;
};

class SquareGenerator : public AbstractGenerator {
 public:
  SquareGenerator() = default;
  void setDcw(double dcw) override;
  double getPhase(double phasetime) override;
};

class PulseGenerator : public AbstractGenerator {
 public:
  PulseGenerator() = default;
  void setDcw(double dcw) override;
  double getPhase(double phasetime) override;
};

class DoubleSineGenerator : public AbstractGenerator {
 public:
  DoubleSineGenerator() = default;
  void setDcw(double dcw) override;
  double getPhase(double phasetime) override;
};

class SawPulseGenerator : public AbstractGenerator {
 public:
  SawPulseGenerator() = default;
  void setDcw(double dcw) override;
  double getPhase(double phasetime) override;
};

class AbstractResonanceGenerator : public AbstractGenerator {
 protected:
  static constexpr double kMaxFreqMult = 14.0;
  double highFreqPhaseCoef_;
  void setDcw(double dcw) override;
  double getPhase(double phasetime) override;
  virtual double getEnvelope(double phasetime) = 0;
  double generate(double phasetime) override;
};

class ResonanceSawToothGenerator : public AbstractResonanceGenerator {
 public:
  ResonanceSawToothGenerator() = default;
  double getEnvelope(double phasetime) override;
};

class ResonanceTriangleGenerator : public AbstractResonanceGenerator {
 public:
  ResonanceTriangleGenerator() = default;
  double getEnvelope(double phasetime) override;
};

class ResonanceTrapezoidGenerator : public AbstractResonanceGenerator {
 public:
  ResonanceTrapezoidGenerator() = default;
  double getEnvelope(double phasetime) override;
};

// Single-voice PD (phase distortion) sound source: oscillator + DCO/DCW/DCA envelopes.
// One instance models one sounding voice; PDProcessor owns a pool of these for polyphony.
class PD {
 private:
  static constexpr double kDcoEgPitchDepth = 3.0;  // TODO: temp impl, not yet a user parameter

  Waveform waveform_;
  double phasetime_;
  double sampleRate_;
  EG dcoEg_;
  EG dcwEg_;
  EG dcaEg_;
  std::unique_ptr<AbstractGenerator> generator_;

 public:
  PD();
  virtual void setWaveform(int8 waveformIndex);
  virtual void setSampleRate(double sampleRate);
  virtual void resetPhase();
  virtual double generate(double freq, bool& isDcaEnd);
  virtual void setupEg();
  virtual void setEgRate(int32 paramId, int32 index, ParamValue rate);
  virtual void setEgLevel(int32 paramId, int32 index, ParamValue level);
  virtual void setEgSustainPoint(int32 paramId, int8 point);
  virtual void setEgEndPoint(int32 paramId, int8 point);
  virtual void restartEg();
  virtual void haltEg();
};

}  // namespace Vst
}  // namespace Steinberg
