#include "controller.h"

#include <cstdio>
#include <cstring>

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "const.h"
#include "config.h"
#include "editor.h"

namespace Steinberg {
namespace Vst {

namespace {

const char* const kWaveformNames[] = {
  "1: Saw Tooth",
  "2: Square",
  "3: Pulse",
  "4: Double Sine",
  "5: Saw Pulse",
  "6: Resonance I Saw Tooth",
  "7: Resonance II Triangle",
  "8: Resonance III Trapezoid",
};

void toString128(String128 dst, const char* src) {
  UString(dst, 128).fromAscii(src);
}

void appendAsciiString(StringListParameter* param, const char* text) {
  String128 str;
  toString128(str, text);
  param->appendString(str);
}

// Registers the waveform selectors and DCO/DCW/DCA EG parameters of one line.
// `linePrefix` is prepended to every title (e.g. "L1"), `lineBase` is the
// line's first parameter id (layout in const.h).
void addLineParameters(ParameterContainer& parameters, const char* linePrefix, int32 lineBase) {
  char buf[64];
  String128 title;

  // waveform (first and second; second adds an "Off" state)
  snprintf(buf, sizeof(buf), "%s Waveform 1st", linePrefix);
  toString128(title, buf);
  StringListParameter* waveformFirst =
      new StringListParameter(title, lineBase + kLineParamWaveformFirst);
  for (const char* name : kWaveformNames) {
    appendAsciiString(waveformFirst, name);
  }
  parameters.addParameter(waveformFirst);

  snprintf(buf, sizeof(buf), "%s Waveform 2nd", linePrefix);
  toString128(title, buf);
  StringListParameter* waveformSecond =
      new StringListParameter(title, lineBase + kLineParamWaveformSecond);
  appendAsciiString(waveformSecond, "Off");
  for (const char* name : kWaveformNames) {
    appendAsciiString(waveformSecond, name);
  }
  parameters.addParameter(waveformSecond);

  // DCO/DCW/DCA EG (order must match EgKind)
  const char* const egNames[] = {"DCO", "DCW", "DCA"};
  for (int32 egIndex = 0; egIndex < 3; egIndex++) {
    int32 egBase = lineBase + kLineParamEgBegin + egIndex * kLineParamEgBlockSize;

    // rates and levels, interleaved for display (Rate 1, Lvl 1, Rate 2, ...)
    for (int32 i = 0; i < kNumEgRateParams; i++) {
      snprintf(buf, sizeof(buf), "%s %s EG Rate %d", linePrefix, egNames[egIndex], i + 1);
      toString128(title, buf);
      parameters.addParameter(new DiscreteRangeParameter(title, egBase + kEgParamRate0 + i));
      if (i < kNumEgLevelParams) {
        snprintf(buf, sizeof(buf), "%s %s EG Lvl %d", linePrefix, egNames[egIndex], i + 1);
        toString128(title, buf);
        parameters.addParameter(new DiscreteRangeParameter(title, egBase + kEgParamLevel0 + i));
      }
    }

    // sustain point: Off, 1..7
    snprintf(buf, sizeof(buf), "%s %s EG Sustain Point", linePrefix, egNames[egIndex]);
    toString128(title, buf);
    StringListParameter* sustainPoint =
        new StringListParameter(title, egBase + kEgParamSustainPoint);
    appendAsciiString(sustainPoint, "Off");
    for (int32 i = 1; i < kNumEgSustainPointOptions; i++) {
      snprintf(buf, sizeof(buf), "%d", i);
      appendAsciiString(sustainPoint, buf);
    }
    parameters.addParameter(sustainPoint);

    // end point: 2..8
    snprintf(buf, sizeof(buf), "%s %s EG End Point", linePrefix, egNames[egIndex]);
    toString128(title, buf);
    StringListParameter* endPoint = new StringListParameter(title, egBase + kEgParamEndPoint);
    for (int32 i = 0; i < kNumEgEndPointOptions; i++) {
      snprintf(buf, sizeof(buf), "%d", i + 2);
      appendAsciiString(endPoint, buf);
    }
    parameters.addParameter(endPoint);
  }
}

}  // namespace

FUnknown* PDController::createInstance(void*) {
  return (IEditController*)new PDController();
}

tresult PLUGIN_API PDController::initialize(FUnknown* context) {
  tresult result = EditController::initialize(context);
  if (result == kResultFalse) {
    return kResultFalse;
  }

  // pitch bend
  Parameter* pitchBend = new Parameter(STR16("Pitch Bend"),  // title
                                       kParamPitchBend,      // tag
                                       nullptr,              // units
                                       0.5                   // default value (normalized)
  );
  parameters.addParameter(pitchBend);

  // volume
  Parameter* volume = new Parameter(STR16("volume"),  // title
                                    kParamVolume,     // tag
                                    nullptr,          // units
                                    0.5               // default value (normalized)
  );
  parameters.addParameter(volume);

  // line select
  StringListParameter* lineSelect = new StringListParameter(STR16("Line Select"), kParamLineSelect);
  lineSelect->appendString(STR16("1"));
  lineSelect->appendString(STR16("2"));
  lineSelect->appendString(STR16("1+1'"));
  lineSelect->appendString(STR16("1+2'"));
  parameters.addParameter(lineSelect);

  // mono/poly (CZ "SOLO" switch)
  StringListParameter* monoPoly = new StringListParameter(STR16("Mono/Poly"), kParamMonoPoly);
  monoPoly->appendString(STR16("Poly"));
  monoPoly->appendString(STR16("Mono"));
  parameters.addParameter(monoPoly);

  // detune of the primed line (octave/note/fine combine into one offset)
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("Detune Octave"), kParamDetuneOctave, nullptr,
    2 * kDetuneOctaveRange, -kDetuneOctaveRange, kDetuneOctaveRange, 0
  ));
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("Detune Note"), kParamDetuneNote, nullptr,
    2 * kDetuneNoteRange, -kDetuneNoteRange, kDetuneNoteRange, 0
  ));
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("Detune Fine"), kParamDetuneFine, nullptr,
    2 * kDetuneFineRange, -kDetuneFineRange, kDetuneFineRange, 0
  ));

  // per-line waveform and EG parameters
  addLineParameters(parameters, "L1", kParamLine1Begin);
  addLineParameters(parameters, "L2", kParamLine2Begin);

  return kResultTrue;
}

IPlugView* PLUGIN_API PDController::createView(FIDString name) {
  if (strcmp(name, ViewType::kEditor) == 0) {
    return new PDEditor(this);
  }
  return nullptr;
}

tresult PLUGIN_API PDController::setParamNormalized(ParamID tag, ParamValue value) {
  tresult result = EditController::setParamNormalized(tag, value);
  if (activeEditor_ != nullptr) {
    activeEditor_->updateControl(tag, value);
  }
  return result;
}

void PDController::setActiveEditor(PDEditor* editor) {
  activeEditor_ = editor;
}

tresult PLUGIN_API PDController::notify(IMessage* message) {
  if (message == nullptr) {
    return kInvalidArgument;
  }

  if (strcmp(message->getMessageID(), kScopeMessageId) == 0) {
    const void* data = nullptr;
    uint32 size = 0;
    if (message->getAttributes()->getBinary(kScopeMessageDataAttr, data, size) == kResultTrue) {
      std::lock_guard<std::mutex> lock(scopeMutex_);
      const float* samples = static_cast<const float*>(data);
      scopeData_.assign(samples, samples + size / sizeof(float));
    }
    return kResultTrue;
  }

  return EditController::notify(message);
}

void PDController::copyScopeData(std::vector<float>& out) {
  std::lock_guard<std::mutex> lock(scopeMutex_);
  out = scopeData_;
}

tresult PLUGIN_API PDController::setComponentState(IBStream* state) {
  if (state == nullptr) {
    return kResultFalse;
  }

  IBStreamer streamer(state, kLittleEndian);
  int32 version;
  if (!streamer.readInt32(version) || version != kStateVersion) {
    return kResultFalse;
  }
  for (int32 paramId = 0; paramId < kNumParams; paramId++) {
    double value;
    if (!streamer.readDouble(value)) {
      return kResultFalse;
    }
    setParamNormalized(paramId, value);
  }
  return kResultTrue;
}

namespace {

// MIDI CC assignment of the EG parameters. Each EG occupies one contiguous
// CC block laid out like the EG parameter sub-block itself (8 rates, 7
// levels, sustain point, end point); the MIDI channel selects the line
// (channel 1 = line 1, channel 2 = line 2). The chosen ranges avoid every
// CC with a conventional meaning (mod wheel, pedals, sound controllers,
// RPN/NRPN, channel mode messages, ...).
constexpr CtrlNumber kEgCcBlockFirst[] = {
  14,   // DCO EG: CC 14-30
  46,   // DCW EG: CC 46-62
  102,  // DCA EG: CC 102-118
};

// Returns the offset within a line parameter block addressed by `cc`,
// or -1 if the CC is not an EG controller.
int32 lineParamOffsetForCc(CtrlNumber cc) {
  for (int32 egIndex = 0; egIndex < 3; egIndex++) {
    CtrlNumber first = kEgCcBlockFirst[egIndex];
    if (first <= cc && cc < first + kLineParamEgBlockSize) {
      return kLineParamEgBegin + egIndex * kLineParamEgBlockSize + (cc - first);
    }
  }
  return -1;
}

}  // namespace

tresult PLUGIN_API PDController::getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                             CtrlNumber midiControllerNumber,
                                                             ParamID& id) {
  // channel-independent assignments
  switch (midiControllerNumber) {
    case kPitchBend:
      id = kParamPitchBend;
      return kResultTrue;
    case kCtrlVolume:  // CC 7, the conventional channel volume
      id = kParamVolume;
      return kResultTrue;
    default:
      break;
  }

  // EG parameters: MIDI channel 1 controls line 1, channel 2 controls line 2
  if (channel == 0 || channel == 1) {
    int32 offset = lineParamOffsetForCc(midiControllerNumber);
    if (offset >= 0) {
      id = (channel == 0 ? kParamLine1Begin : kParamLine2Begin) + offset;
      return kResultTrue;
    }
  }

  return kResultFalse;
}

}  // namespace Vst
}  // namespace Steinberg
