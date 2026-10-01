#include "controller.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/vstpresetfile.h"

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

  // dummy parameter to receive CC 126 (Mono Mode On)
  Parameter* monoTrigger = new Parameter(STR16("Mono Trigger"), kParamMonoTrigger, nullptr, 0.0);
  monoTrigger->getInfo().flags = ParameterInfo::kIsHidden;
  parameters.addParameter(monoTrigger);
  // dummy parameter to receive CC 127 (Poly Mode On)
  Parameter* polyTrigger = new Parameter(STR16("Poly Trigger"), kParamPolyTrigger, nullptr, 0.0);
  polyTrigger->getInfo().flags = ParameterInfo::kIsHidden;
  parameters.addParameter(polyTrigger);

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

  // CC edit target: which line the EG MIDI CC blocks address
  StringListParameter* ccEditLine = new StringListParameter(STR16("CC Edit Line"),
                                                            kParamCcEditLine);
  ccEditLine->appendString(STR16("Line 1"));
  ccEditLine->appendString(STR16("Line 2"));
  parameters.addParameter(ccEditLine);

  // octave range (CZ OCTAVE RANGE), shifts both lines and the key follow note
  StringListParameter* octaveRange = new StringListParameter(STR16("Octave Range"),
                                                             kParamOctaveRange);
  octaveRange->appendString(STR16("-1"));
  octaveRange->appendString(STR16("0"));
  octaveRange->appendString(STR16("+1"));
  octaveRange->getInfo().defaultNormalizedValue = 0.5;
  octaveRange->setNormalized(0.5);
  parameters.addParameter(octaveRange);

  // DCA key follow per line: faster DCA envelopes on higher notes
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("L1 DCA Key Follow"), kParamLine1DcaKeyFollow, nullptr,
    kNumKeyFollowOptions - 1, 0, kNumKeyFollowOptions - 1, 0
  ));
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("L2 DCA Key Follow"), kParamLine2DcaKeyFollow, nullptr,
    kNumKeyFollowOptions - 1, 0, kNumKeyFollowOptions - 1, 0
  ));

  // master tune in cents (applies to both lines)
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("Master Tune"), kParamMasterTune, STR16("cent"),
    2 * kMasterTuneRangeCents, -kMasterTuneRangeCents, kMasterTuneRangeCents, 0
  ));

  // DCW key follow per line: lower DCW levels on higher notes
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("L1 DCW Key Follow"), kParamLine1DcwKeyFollow, nullptr,
    kNumKeyFollowOptions - 1, 0, kNumKeyFollowOptions - 1, 0
  ));
  parameters.addParameter(new DiscreteRangeParameter(
    STR16("L2 DCW Key Follow"), kParamLine2DcwKeyFollow, nullptr,
    kNumKeyFollowOptions - 1, 0, kNumKeyFollowOptions - 1, 0
  ));

  return kResultTrue;
}

IPlugView* PLUGIN_API PDController::createView(FIDString name) {
  if (strcmp(name, ViewType::kEditor) == 0) {
    return new PDEditor(this);
  }
  return nullptr;
}

tresult PLUGIN_API PDController::setParamNormalized(ParamID tag, ParamValue value) {
  ParamValue previous = getParamNormalized(tag);
  tresult result = EditController::setParamNormalized(tag, value);
  if (activeEditor_ != nullptr) {
    activeEditor_->updateControl(tag, value);
  }

  // Mono/Poly Mode
  if ((tag == kParamMonoTrigger || tag == kParamPolyTrigger) && value > 0.0 ){
    ParamValue monoPolyValue = (tag == kParamMonoTrigger)? 1.0 : 0.0;
    beginEdit(kParamMonoPoly);
    setParamNormalized(kParamMonoPoly, monoPolyValue);
    performEdit(kParamMonoPoly, monoPolyValue);
    endEdit(kParamMonoPoly);

    // Reset the trigger itself so the next press (even with the same CC
    // value) is seen as a change from 0, not a no-op.
    EditController::setParamNormalized(tag, 0.0);
  }

  // The EG CC blocks route to the line chosen by CC Edit Line,
  // so the host must re-query the MIDI CC mapping whenever it changes.
  if (tag == kParamCcEditLine && previous != value && componentHandler != nullptr) {
    componentHandler->restartComponent(kMidiCCAssignmentChanged);
  }
  return result;
}

void PDController::setActiveEditor(PDEditor* editor) {
  activeEditor_ = editor;
}

void PDController::applyParamFromProcessor(ParamID id, ParamValue value) {
  // Already known: nothing to redraw.
  if (getParamNormalized(id) == value) {
    return;
  }

  // Updates the parameter and, through it, the editor; for the Mono/Poly
  // triggers this is also what runs the trigger handling.
  //
  // Deliberately does not call performEdit: two UI-driven changes to the same
  // parameter that land in separate process() calls (any two clicks a host
  // buffer length or more apart, which at typical buffer sizes is most
  // clicks) would otherwise bounce forever. The processor echoes back
  // whichever value it just received; if that echo were reported to the host
  // again here, the host would deliver it to the processor once more on the
  // next block, which would echo it back once more, and so on -- the two
  // values would keep swapping instead of ever settling. The UI only ever
  // needs the controller's own value (set here) to stay right; the host's
  // automation/generic display not tracking MIDI-CC-driven changes is the
  // trade-off for that.
  setParamNormalized(id, value);
}

tresult PLUGIN_API PDController::notify(IMessage* message) {
  if (message == nullptr) {
    return kInvalidArgument;
  }

  // data exchange blocks, when the host has no data exchange of its own
  if (dataExchange_.onMessage(message)) {
    return kResultTrue;
  }
  return EditController::notify(message);
}

void PLUGIN_API PDController::queueOpened(DataExchangeUserContextID userContextID,
                                          uint32 blockSize, TBool& dispatchOnBackgroundThread) {
  dispatchOnBackgroundThread = false;  // the parameter echoes update the UI
}

void PLUGIN_API PDController::queueClosed(DataExchangeUserContextID userContextID) {
}

void PLUGIN_API PDController::onDataExchangeBlocksReceived(DataExchangeUserContextID userContextID,
                                                           uint32 numBlocks,
                                                           DataExchangeBlock* blocks,
                                                           TBool onBackgroundThread) {
  for (uint32 i = 0; i < numBlocks; i++) {
    const DataExchangeBlock& block = blocks[i];
    if (userContextID == kScopeExchangeId && block.size >= sizeof(float) * kScopeFrameSize) {
      std::lock_guard<std::mutex> lock(scopeMutex_);
      const float* samples = static_cast<const float*>(block.data);
      scopeData_.assign(samples, samples + kScopeFrameSize);
    } else if (userContextID == kParamSyncExchangeId && block.size >= sizeof(ParamSyncBlock)) {
      // Parameter values the processor received directly (MIDI CC mapped
      // through IMidiMapping never reaches the controller by itself).
      const ParamSyncBlock* sync = static_cast<const ParamSyncBlock*>(block.data);
      for (uint32 entry = 0; entry < sync->count && entry < kNumParams; entry++) {
        applyParamFromProcessor(sync->entries[entry].id, sync->entries[entry].value);
      }
    }
  }
}

void PDController::copyScopeData(std::vector<float>& out) {
  std::lock_guard<std::mutex> lock(scopeMutex_);
  out = scopeData_;
}

tresult PLUGIN_API PDController::setComponentState(IBStream* state) {
  if (state == nullptr) {
    return kResultFalse;
  }
  // A state restored by the host (e.g. its own preset browser) is no longer
  // the current preset file; on project load, setState() restores the path
  // right after this. loadPresetFile() sets it again after restoring.
  setCurrentPresetPath({});

  IBStreamer streamer(state, kLittleEndian);
  int32 version;
  if (!streamer.readInt32(version)) {
    return kResultFalse;
  }
  int32 numParams = numParamsOfStateVersion(version);
  if (numParams == 0) {
    return kResultFalse;
  }
  for (int32 paramId = 0; paramId < numParams; paramId++) {
    double value;
    if (!streamer.readDouble(value)) {
      return kResultFalse;
    }
    setParamNormalized(paramId, value);
  }
  // parameters appended after the stream's version start from their defaults
  for (int32 paramId = numParams; paramId < kNumParams; paramId++) {
    if (Parameter* parameter = getParameterObject(paramId)) {
      setParamNormalized(paramId, parameter->getInfo().defaultNormalizedValue);
    }
  }
  return kResultTrue;
}

// Version tag of the controller's own (UI) state stream.
// v2 appended the current preset file (UTF-8, length-prefixed).
namespace {
constexpr int32 kUiStateVersion = 2;
constexpr int32 kMaxPresetPathSize = 32768;
}  // namespace

tresult PLUGIN_API PDController::getState(IBStream* state) {
  if (state == nullptr) {
    return kResultFalse;
  }
  IBStreamer streamer(state, kLittleEndian);
  const std::string presetPath = currentPresetPath_.u8string();
  const int32 presetPathSize = static_cast<int32>(presetPath.size());
  if (!streamer.writeInt32(kUiStateVersion) || !streamer.writeInt32(skinIndex_)
      || !streamer.writeInt32(presetPathSize)
      || streamer.writeRaw(presetPath.data(), presetPathSize) != presetPathSize) {
    return kResultFalse;
  }
  return kResultTrue;
}

tresult PLUGIN_API PDController::setState(IBStream* state) {
  if (state == nullptr) {
    return kResultFalse;
  }
  IBStreamer streamer(state, kLittleEndian);
  int32 version;
  int32 skinIndex;
  if (!streamer.readInt32(version) || version < 1 || kUiStateVersion < version
      || !streamer.readInt32(skinIndex)) {
    return kResultFalse;
  }
  skinIndex_ = skinIndex;

  if (version >= 2) {
    int32 size;
    if (!streamer.readInt32(size) || size < 0 || kMaxPresetPathSize < size) {
      return kResultFalse;
    }
    std::string presetPath(static_cast<size_t>(size), '\0');
    if (streamer.readRaw(presetPath.data(), size) != size) {
      return kResultFalse;
    }
    setCurrentPresetPath(std::filesystem::u8path(presetPath));
  }
  return kResultTrue;
}

void PDController::setSkinIndex(int32 index) {
  skinIndex_ = index;
}

int32 PDController::getSkinIndex() const {
  return skinIndex_;
}

// Preset files are read and written through std::fstream rather than the
// SDK's FileStream, whose fopen() cannot open non-ASCII paths on Windows.

bool PDController::savePresetFile(const std::filesystem::path& path) {
  // synthesize the processor state stream from the current parameter values
  MemoryStream componentState;
  IBStreamer streamer(&componentState, kLittleEndian);
  bool ok = streamer.writeInt32(kStateVersion);
  for (int32 paramId = 0; ok && paramId < kNumParams; paramId++) {
    ok = streamer.writeDouble(getParamNormalized(paramId));
  }
  MemoryStream preset;
  if (ok) {
    componentState.seek(0, IBStream::kIBSeekSet, nullptr);
    ok = PresetFile::savePreset(&preset, ProcessorUID, &componentState);
  }
  if (!ok) {
    return false;
  }

  std::ofstream file(path, std::ios::binary);
  file.write(preset.getData(), static_cast<std::streamsize>(preset.getSize()));
  if (!file) {
    return false;
  }
  setCurrentPresetPath(path);
  return true;
}

bool PDController::loadPresetFile(std::filesystem::path path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
  MemoryStream stream(bytes.data(), static_cast<TSize>(bytes.size()));

  PresetFile presetFile(&stream);
  if (!presetFile.readChunkList()
      || !presetFile.restoreComponentState(static_cast<IEditController*>(this))) {
    return false;
  }
  setCurrentPresetPath(path);

  // the processor switches to the whole preset at once (see kPresetMessageId)
  std::array<ParamValue, kNumParams> values;
  for (int32 paramId = 0; paramId < kNumParams; paramId++) {
    values[paramId] = getParamNormalized(paramId);
  }
  if (IMessage* message = allocateMessage()) {
    message->setMessageID(kPresetMessageId);
    message->getAttributes()->setBinary(kPresetMessageDataAttr, values.data(),
                                        static_cast<uint32>(sizeof(values)));
    sendMessage(message);
    message->release();
  }

  // report the restored values to the host (the processor already has them)
  for (int32 paramId = 0; paramId < kNumParams; paramId++) {
    ParamValue value = getParamNormalized(paramId);
    beginEdit(paramId);
    performEdit(paramId, value);
    endEdit(paramId);
  }
  return true;
}

const std::filesystem::path& PDController::getCurrentPresetPath() const {
  return currentPresetPath_;
}

PresetLibrary& PDController::presetLibrary() {
  return presetLibrary_;
}

PDController::BrowserState& PDController::browserState() {
  return browserState_;
}

void PDController::rescanPresetFolders() {
  presetLibrary_.rescan();
  if (activeEditor_ != nullptr) {
    activeEditor_->presetFoldersChanged();
  }
}

bool PDController::addPresetFolder(const std::filesystem::path& root) {
  if (!presetLibrary_.addRoot(root)) {
    return false;
  }
  const std::vector<PresetFolder>& folders = presetLibrary_.folders();
  for (size_t i = 0; i < folders.size(); i++) {
    const std::filesystem::path& directory = folders[i].directory;
    if (isSamePath(directory, root) || isSamePath(directory.parent_path(), root)) {
      browserState_.folderIndex = static_cast<int32>(i);
      break;
    }
  }
  if (activeEditor_ != nullptr) {
    activeEditor_->presetFoldersChanged();
  }
  return true;
}

void PDController::setCurrentPresetPath(const std::filesystem::path& path) {
  currentPresetPath_ = path;
  if (activeEditor_ != nullptr) {
    activeEditor_->currentPresetChanged();
  }
}

namespace {
// CCs with no conventional MIDI meaning, repurposed for edit-target parameters
// line selection to be edited by CC (values 0..63 = line 1, 64..127 = line 2)
constexpr CtrlNumber kCcEditLineController = 3;
// line selection to be played (values 0..127 -> {1, 2, 1+1', 1+2'})
constexpr CtrlNumber kLineSelect = 9;
// detune
constexpr CtrlNumber kDetuneOctave = 85;
constexpr CtrlNumber kDetuneNote = 86;
constexpr CtrlNumber kDetuneFine = 87;
// first waveform selection (values 0..127 -> waveform {1..8})
constexpr CtrlNumber kCcEditWaveformFirst = 89;
// second waveform selection (values 0..127 -> waveform {Off, 1..8})
constexpr CtrlNumber kCcEditWaveformSecond = 90;
// octave range (values 0..127 -> {-1, 0, +1}); right after the DCO EG block
constexpr CtrlNumber kCcOctaveRange = 31;
// master tune (values 0..127 -> -100..+100 cents); General Purpose 5, no conventional meaning
constexpr CtrlNumber kCcMasterTune = 80;
// DCW key follow of the CC edit line (values 0..127 -> 0..9); right after the DCW EG block
constexpr CtrlNumber kCcEditDcwKeyFollow = 63;
// DCA key follow of the CC edit line (values 0..127 -> 0..9); right after the DCA EG block
constexpr CtrlNumber kCcEditDcaKeyFollow = 119;
// mono mode on
constexpr CtrlNumber kMonoModeOn = 126;
// poly mode on
constexpr CtrlNumber kPolyModeOn = 127;

// MIDI CC assignment of the EG parameters.
// Each EG occupies one contiguous CC block laid out like
// the EG parameter sub-block itself (8 rates, 7 levels, sustain point, end point);
// the line the block addresses is chosen by the CC Edit Line parameter,
// as on hardware where the panel selects the line being edited.
// The chosen ranges avoid every CC with a conventional meaning
// (mod wheel, pedals, sound controllers, RPN/NRPN, channel mode messages, ...).
constexpr CtrlNumber kEgCcBlockFirst[] = {
  14,   // DCO EG: CC 14-30
  46,   // DCW EG: CC 46-62
  102,  // DCA EG: CC 102-118
};

int32 lineParamBase(ParamValue lineSelCcValue){
    return (lineSelCcValue < 0.5 ? kParamLine1Begin : kParamLine2Begin);
}

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
  switch (midiControllerNumber) {
    case kPitchBend:
      id = kParamPitchBend;
      return kResultTrue;
    case kCtrlVolume:  // CC 7, the conventional channel volume
      id = kParamVolume;
      return kResultTrue;
    case kCcEditLineController:
      id = kParamCcEditLine;
      return kResultTrue;
    case kLineSelect:
      id = kParamLineSelect;
      return kResultTrue;
    case kMonoModeOn:
      id = kParamMonoTrigger;
      return kResultTrue;
    case kPolyModeOn:
      id = kParamPolyTrigger;
      return kResultTrue;
    case kDetuneOctave:
      id = kParamDetuneOctave;
      return kResultTrue;
    case kDetuneNote:
      id = kParamDetuneNote;
      return kResultTrue;
    case kDetuneFine:
      id = kParamDetuneFine;
      return kResultTrue;
    case kCcOctaveRange:
      id = kParamOctaveRange;
      return kResultTrue;
    case kCcMasterTune:
      id = kParamMasterTune;
      return kResultTrue;
    case kCcEditDcwKeyFollow:
      id = getParamNormalized(kParamCcEditLine) < 0.5 ? kParamLine1DcwKeyFollow
                                                      : kParamLine2DcwKeyFollow;
      return kResultTrue;
    case kCcEditDcaKeyFollow:
      id = getParamNormalized(kParamCcEditLine) < 0.5 ? kParamLine1DcaKeyFollow
                                                      : kParamLine2DcaKeyFollow;
      return kResultTrue;
    default:
      break;
  }

  // resolve the line-1/2 base id from the CC edit line parameter value
  int32 idOffset = lineParamBase(getParamNormalized(kParamCcEditLine));

  if (midiControllerNumber == kCcEditWaveformFirst) {
    id = idOffset + kLineParamWaveformFirst;
    return kResultTrue;
  } else if (midiControllerNumber == kCcEditWaveformSecond) {
    id = idOffset + kLineParamWaveformSecond;
    return kResultTrue;
  }

  // EG parameters address the line currently chosen by CC Edit Line
  int32 lineParamOffset = lineParamOffsetForCc(midiControllerNumber);
  if (lineParamOffset >= 0) {
    id = idOffset + lineParamOffset;
    return kResultTrue;
  }

  return kResultFalse;
}

}  // namespace Vst
}  // namespace Steinberg
