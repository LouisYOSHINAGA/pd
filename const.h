#pragma once

namespace Steinberg {
namespace Vst {

constexpr double kDefaultSampleRate = 44100.0;
constexpr double kA4Note = 69.0;
constexpr double kA4Freq = 440.0;
constexpr double kEpsilon = 0.00001;

constexpr int kNumEgSustainPointOptions = 8;
constexpr int kNumEgEndPointOptions = 7;

// Maximum number of simultaneously sounding voices. When a note-on arrives
// with no free voice available, the oldest allocated voice is stolen.
constexpr int kMaxVoices = 16;

// Fixed per-mix headroom applied to the voice sum (1/sqrt(kMaxVoices)).
// The usual polyphonic-synth approach: a constant gain independent of how
// many voices happen to be sounding, so note starts/ends never modulate the
// loudness of other held notes.
constexpr double kVoiceMixGain = 0.25;

enum ParamId {
  // System
  kParamPitchBend,
  kParamVolume,

  // DCO
  kParamWaveform,

  // DCO EG
  kParamDcoEgRate0,
  kParamDcoEgRate1,
  kParamDcoEgRate2,
  kParamDcoEgRate3,
  kParamDcoEgRate4,
  kParamDcoEgRate5,
  kParamDcoEgRate6,
  kParamDcoEgRate7,
  kParamDcoEgLevel0,
  kParamDcoEgLevel1,
  kParamDcoEgLevel2,
  kParamDcoEgLevel3,
  kParamDcoEgLevel4,
  kParamDcoEgLevel5,
  kParamDcoEgLevel6,
  kParamDcoEgSustainPoint,
  kParamDcoEgEndPoint,

  // DCW EG
  kParamDcwEgRate0,
  kParamDcwEgRate1,
  kParamDcwEgRate2,
  kParamDcwEgRate3,
  kParamDcwEgRate4,
  kParamDcwEgRate5,
  kParamDcwEgRate6,
  kParamDcwEgRate7,
  kParamDcwEgLevel0,
  kParamDcwEgLevel1,
  kParamDcwEgLevel2,
  kParamDcwEgLevel3,
  kParamDcwEgLevel4,
  kParamDcwEgLevel5,
  kParamDcwEgLevel6,
  kParamDcwEgSustainPoint,
  kParamDcwEgEndPoint,

  // DCA EG
  kParamDcaEgRate0,
  kParamDcaEgRate1,
  kParamDcaEgRate2,
  kParamDcaEgRate3,
  kParamDcaEgRate4,
  kParamDcaEgRate5,
  kParamDcaEgRate6,
  kParamDcaEgRate7,
  kParamDcaEgLevel0,
  kParamDcaEgLevel1,
  kParamDcaEgLevel2,
  kParamDcaEgLevel3,
  kParamDcaEgLevel4,
  kParamDcaEgLevel5,
  kParamDcaEgLevel6,
  kParamDcaEgSustainPoint,
  kParamDcaEgEndPoint,

  kNumParams
};

enum class Waveform {
  kSawTooth,
  kSquare,
  kPulse,
  kDoubleSine,
  kSawPulse,
  kResonanceSawTooth,
  kResonanceTriangle,
  kResonanceTrapezoid,
  kNumWaveforms
};

}  // namespace Vst
}  // namespace Steinberg
