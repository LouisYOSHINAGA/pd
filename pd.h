#pragma once

#include <array>
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

// The three envelope generators of one PD line. The enumerator order matches
// the EG sub-block order of the line parameter layout in const.h.
enum class EgKind {
  kDco = 0,
  kDcw,
  kDca,
  kNumEgKinds
};

// One PD (phase distortion) sound source line: oscillator + DCO/DCW/DCA envelopes.
// As on the CZ series, a second waveform can be selected in addition to the
// first; the oscillator then alternates between the two on successive cycles,
// which doubles the waveform period (adding a sub-harmonic one octave down).
class PD {
 private:
  static constexpr double kDcoEgPitchDepth = 3.0;  // TODO: temp impl, not yet a user parameter

  double phasetime_;
  double sampleRate_;
  bool onSecondWaveform_;
  std::array<EG, static_cast<int>(EgKind::kNumEgKinds)> egs_;
  std::unique_ptr<AbstractGenerator> generatorFirst_;
  std::unique_ptr<AbstractGenerator> generatorSecond_;  // null while second is Off

  EG& eg(EgKind kind) { return egs_[static_cast<int>(kind)]; }

 public:
  PD();
  virtual void setWaveformFirst(int8 waveformIndex);
  virtual void setWaveformSecond(int8 selection);  // 0 = Off, 1..8 = waveform index + 1
  virtual void setSampleRate(double sampleRate);
  virtual void resetPhase();
  virtual double generate(double freq, bool& isDcaEnd);
  virtual void setupEg();
  virtual void setEgRate(EgKind kind, int32 index, ParamValue rate);
  virtual void setEgLevel(EgKind kind, int32 index, ParamValue level);
  virtual void setEgSustainPoint(EgKind kind, int8 point);
  virtual void setEgEndPoint(EgKind kind, int8 point);
  virtual void restartEg();
  virtual void haltEg();
};

}  // namespace Vst
}  // namespace Steinberg
