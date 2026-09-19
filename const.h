#pragma once

#include <cstdint>

namespace Steinberg {
namespace Vst {

// Internal DSP tick rate, fixed to match the real CZ hardware's internal
// clock. Independent of the host's output sample rate; PDProcessor resamples
// the internally-generated ticks up to whatever rate the host requests.
constexpr double kInternalSampleRate = 44100.0;
constexpr double kA4Note = 69.0;
constexpr double kA4Freq = 440.0;
constexpr double kEpsilon = 0.00001;

// Version tag written at the head of the processor state stream. Every
// version so far only appended parameters, so an older stream is a prefix of
// the current one and stays readable (see paramCountForStateVersion).
//   v2: appended kParamCcEditLine
//   v3: appended the Mono/Poly CC triggers
//   v4: appended octave shift, modulation and the vibrato block
constexpr int kStateVersion = 4;

// Oscilloscope: the processor streams frames of recent output samples to the
// controller as messages; the editor's scope view renders the latest frame.
constexpr int kScopeFrameSize = 1024;
constexpr const char* kScopeMessageId = "oscilloscope";
constexpr const char* kScopeMessageDataAttr = "data";

// Parameter feedback: a MIDI CC mapped through IMidiMapping is turned into a
// parameter change by the host and delivered to the processor only; whether
// the edit controller is told about it is host dependent, and most hosts do
// not. The processor therefore echoes every parameter change it receives back
// to the controller, which updates the UI.
constexpr const char* kParamSyncMessageId = "paramsync";
constexpr const char* kParamSyncMessageDataAttr = "data";

// One echoed parameter value. Sent as a raw array, so both sides must be
// built together (they always are: same binary).
struct ParamSyncEntry {
  int32_t id;
  double value;
};

// Decodes the normalized value of a discrete parameter with `numOptions`
// states into its option index (0 .. numOptions-1). Splits the normalized
// range into numOptions equal-width buckets, so a continuously-varying
// source (MIDI CC, host automation) lands on every option with the same
// odds; this also reproduces index exactly for every normalized value a
// control sends by the index/(numOptions-1) convention (menus, segment
// buttons, StringListParameter), since index * numOptions/(numOptions-1)
// only reaches the next integer when index is the last option, and the
// clamp catches that case.
inline int decodeOptionIndex(double normalized, int numOptions) {
  int index = static_cast<int>(normalized * numOptions + kEpsilon);
  return index < numOptions ? index : numOptions - 1;
}

// Decodes the normalized value of a symmetric signed discrete parameter
// into its plain value (-range .. +range).
inline int decodeSignedOption(double normalized, int range) {
  return decodeOptionIndex(normalized, 2 * range + 1) - range;
}

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

// CZ OCTAVE (the panel's "octave range"): transposes the whole keyboard by
// one octave. The hardware stores it in the OCTV bits of the sysex PFLAG byte,
// which only encode 0 / +1 / -1, so the range is one octave either way.
constexpr int kOctaveShiftRange = 1;

// CZ MODULATION: a global switch sitting next to LINE SELECT on the panel.
// The hardware packs it into the same sysex byte as the DCO1 waveform (MFW),
// where "no modulation", "ring modulation" and "noise modulation" are mutually
// exclusive; it only has an effect in the dual-line modes (1+1', 1+2'), since
// it needs two lines to work on. Noise modulation is not implemented yet.
enum class Modulation {
  kOff,
  kRing,  // the two lines are multiplied instead of summed
  kNumModulations
};

// Number of states of the second-waveform selector: "Off" + the 8 waveforms.
constexpr int kNumSecondWaveformOptions = 9;

// CZ VIBRATO: one global LFO modulating the pitch of every line, with the
// four panel parameters WAVE / DELAY / RATE / DEPTH.
enum class VibratoWave {
  kTriangle,
  kSawUp,
  kSawDown,
  kSquare,
  kNumVibratoWaves
};

// DELAY, RATE and DEPTH are 0..99 dials, like the EG rates and levels.
constexpr int kVibratoDialMax = 99;

// Layout of one line's parameter block, relative to the line's base id.
// The three EG sub-blocks are ordered DCO, DCW, DCA (must match EgKind).
constexpr int kLineParamWaveformFirst = 0;
constexpr int kLineParamWaveformSecond = 1;
constexpr int kLineParamEgBegin = 2;  // = the number of waveform selection parameter
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

  // Selects which line the EG MIDI CC blocks address (appended in state v2).
  kParamCcEditLine = kParamLine2Begin + kNumLineParams,

  // Mono/Poly Mode
  kParamMonoTrigger,  // dummy to receive CC 126 (Mono Mode On)
  kParamPolyTrigger,  // dummy to receive CC 127 (Poly Mode On)

  // Appended in state v4. New parameters always go at the end so that an
  // older state stream stays a valid prefix of the current one.
  kParamOctaveShift,
  kParamModulation,
  kParamVibratoWave,
  kParamVibratoDelay,
  kParamVibratoRate,
  kParamVibratoDepth,

  kNumParams
};

// How many parameters a state stream of a given version carries. Older
// versions stop short of kNumParams; whatever they leave out is restored to
// its default value. Returns -1 for a version this build cannot read.
inline int32_t paramCountForStateVersion(int32_t version) {
  switch (version) {
    case 1:  return kParamCcEditLine;    // predates the CC edit line selector
    case 2:  return kParamMonoTrigger;   // predates the Mono/Poly CC triggers
    case 3:  return kParamOctaveShift;   // predates octave/modulation/vibrato
    case 4:  return kNumParams;
    default: return -1;
  }
}

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
