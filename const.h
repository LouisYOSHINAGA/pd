#pragma once

namespace Steinberg {
namespace Vst {

constexpr double kDefaultSampleRate = 44100.0;
constexpr double kA4Note = 69.0;
constexpr double kA4Freq = 440.0;
constexpr double kEpsilon = 0.00001;

constexpr int kNumEgRateParams = 8;
constexpr int kNumEgLevelParams = 7;
constexpr int kNumEgSustainPointOptions = 8;
constexpr int kNumEgEndPointOptions = 7;

// Maximum number of simultaneously sounding voices. When a note-on arrives
// with no free voice available, the oldest allocated voice is stolen.
// As on the CZ series, dual-line modes (1+1', 1+2') halve the polyphony.
constexpr int kMaxVoices = 16;

// Fixed per-mix headroom applied to the voice sum (1/sqrt(kMaxVoices)).
// The usual polyphonic-synth approach: a constant gain independent of how
// many voices happen to be sounding, so note starts/ends never modulate the
// loudness of other held notes.
constexpr double kVoiceMixGain = 0.25;

// CZ LINE SELECT. A "line" is a complete PD sound source (waveform pair +
// DCO/DCW/DCA envelopes). The primed lines (1', 2') are detuned by DETUNE.
enum class LineSelect {
  kLine1,             // line 1 only, full polyphony
  kLine2,             // line 2 only, full polyphony
  kLine1Plus1Detuned, // 1+1': line 1 + detuned copy of line 1, half polyphony
  kLine1Plus2Detuned, // 1+2': line 1 + detuned line 2, half polyphony
  kNumLineSelects
};

// CZ DETUNE ranges: octave/note/fine combine into one signed offset applied
// to the primed line. One fine step is 1/60 halftone (~1.7 cents).
constexpr int kDetuneOctaveRange = 3;
constexpr int kDetuneNoteRange = 11;
constexpr int kDetuneFineRange = 60;
constexpr double kDetuneFineStepCents = 100.0 / kDetuneFineRange;

// Number of states of the second-waveform selector: "Off" + the 8 waveforms.
constexpr int kNumSecondWaveformOptions = 9;

// Layout of one line's parameter block, relative to the line's base id.
// The three EG sub-blocks are ordered DCO, DCW, DCA (must match EgKind).
constexpr int kLineParamWaveformFirst = 0;
constexpr int kLineParamWaveformSecond = 1;
constexpr int kLineParamEgBegin = 2;
constexpr int kLineParamEgBlockSize = kNumEgRateParams + kNumEgLevelParams + 2;  // rates, levels, sustain, end
constexpr int kNumLineParams = kLineParamEgBegin + 3 * kLineParamEgBlockSize;
// Offsets within one EG sub-block.
constexpr int kEgParamRate0 = 0;
constexpr int kEgParamLevel0 = kNumEgRateParams;
constexpr int kEgParamSustainPoint = kNumEgRateParams + kNumEgLevelParams;
constexpr int kEgParamEndPoint = kEgParamSustainPoint + 1;

enum ParamId {
  // System
  kParamPitchBend,
  kParamVolume,
  kParamLineSelect,
  kParamMonoPoly,  // CZ "SOLO" switch: poly / mono (last-note priority)
  kParamDetuneOctave,
  kParamDetuneNote,
  kParamDetuneFine,

  // Per-line blocks (waveform first/second + DCO/DCW/DCA EG each).
  kParamLine1Begin,
  kParamLine2Begin = kParamLine1Begin + kNumLineParams,

  kNumParams = kParamLine2Begin + kNumLineParams
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
