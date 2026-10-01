#pragma once

#include <filesystem>
#include <mutex>
#include <vector>

#include "pluginterfaces/vst/ivstdataexchange.h"
#include "public.sdk/source/vst/utility/dataexchange.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#include "presetlibrary.h"

namespace Steinberg {
namespace Vst {

class PDEditor;

class DiscreteRangeParameter : public RangeParameter {
 public:
  DiscreteRangeParameter(const TChar* title, ParamID tag, const TChar* units = 0,
                         int32 stepCount = 99, ParamValue minPlain = 0,
                         ParamValue maxPlain = 99, ParamValue defaultValuePlain = 0,
                         int32 flags = ParameterInfo::kCanAutomate, UnitID unitID = kRootUnitId)
      : RangeParameter(title, tag, units, minPlain, maxPlain, defaultValuePlain,
                       stepCount, flags, unitID) {
    setPrecision(0);
  }
};

class PDController : public EditController, public IMidiMapping, public IDataExchangeReceiver {
 public:
  static FUnknown* createInstance(void*);
  tresult PLUGIN_API initialize(FUnknown* context) override;
  tresult PLUGIN_API setComponentState(IBStream* state) override;
  tresult PLUGIN_API getState(IBStream* state) override;  // UI state (skin, current preset)
  tresult PLUGIN_API setState(IBStream* state) override;
  tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value) override;
  tresult PLUGIN_API notify(IMessage* message) override;
  IPlugView* PLUGIN_API createView(FIDString name) override;
  tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                 CtrlNumber midiControllerNumber,
                                                 ParamID& id) override;

  // IDataExchangeReceiver: the processor's oscilloscope frames and parameter
  // echoes (see kScopeExchangeId, kParamSyncExchangeId), on the UI thread.
  void PLUGIN_API queueOpened(DataExchangeUserContextID userContextID, uint32 blockSize,
                              TBool& dispatchOnBackgroundThread) override;
  void PLUGIN_API queueClosed(DataExchangeUserContextID userContextID) override;
  void PLUGIN_API onDataExchangeBlocksReceived(DataExchangeUserContextID userContextID,
                                               uint32 numBlocks, DataExchangeBlock* blocks,
                                               TBool onBackgroundThread) override;

  // editor cooperation
  void setActiveEditor(PDEditor* editor);
  // Copies the latest oscilloscope frame received from the processor.
  void copyScopeData(std::vector<float>& out);
  void setSkinIndex(int32 index);
  int32 getSkinIndex() const;

  // Saves/loads all parameters as a standard .vstpreset file. Loading also
  // pushes every parameter to the host/processor. On success the file becomes
  // the current preset (cleared when the host restores another state).
  bool savePresetFile(const std::filesystem::path& path);
  // (`path` by value: restoring clears the current preset path it may refer to)
  bool loadPresetFile(std::filesystem::path path);
  const std::filesystem::path& getCurrentPresetPath() const;

  // Preset browser: the preset folders and what the editor shows, kept here
  // so they survive the editor being closed or rebuilt.
  struct BrowserState {
    bool shown = false;     // browser page instead of the line panels
    int32 folderIndex = 0;  // shown folder (tab)
  };
  PresetLibrary& presetLibrary();
  BrowserState& browserState();
  // Rescans the preset folders and rebuilds the editor's browser.
  void rescanPresetFolders();
  // Adds `root` to the preset folder list and shows its first tab. Returns
  // false if the list file could not be written.
  bool addPresetFolder(const std::filesystem::path& root);

  OBJ_METHODS(PDController, EditController)
  DEFINE_INTERFACES
  DEF_INTERFACE(IMidiMapping)
  DEF_INTERFACE(IDataExchangeReceiver)
  END_DEFINE_INTERFACES(EditController)
  REFCOUNT_METHODS(EditController)

 private:
  PDEditor* activeEditor_ = nullptr;
  DataExchangeReceiverHandler dataExchange_{this};
  std::mutex scopeMutex_;
  std::vector<float> scopeData_;
  int32 skinIndex_ = 0;
  std::filesystem::path currentPresetPath_;
  PresetLibrary presetLibrary_;
  BrowserState browserState_;

  // Applies one parameter value echoed by the processor: updates the UI and,
  // for parameters the host tracks, reports the new value to the host.
  void applyParamFromProcessor(ParamID id, ParamValue value);
  void setCurrentPresetPath(const std::filesystem::path& path);
};

}  // namespace Vst
}  // namespace Steinberg
