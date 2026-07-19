#pragma once

#include <mutex>
#include <vector>
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

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

class PDController : public EditController, public IMidiMapping {
 public:
  static FUnknown* createInstance(void*);
  tresult PLUGIN_API initialize(FUnknown* context) override;
  tresult PLUGIN_API setComponentState(IBStream* state) override;
  tresult PLUGIN_API getState(IBStream* state) override;  // UI preferences (skin)
  tresult PLUGIN_API setState(IBStream* state) override;
  tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value) override;
  tresult PLUGIN_API notify(IMessage* message) override;
  IPlugView* PLUGIN_API createView(FIDString name) override;
  tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                 CtrlNumber midiControllerNumber,
                                                 ParamID& id) override;

  // editor cooperation
  void setActiveEditor(PDEditor* editor);
  // Copies the latest oscilloscope frame received from the processor.
  void copyScopeData(std::vector<float>& out);
  void setSkinIndex(int32 index);
  int32 getSkinIndex() const;

  OBJ_METHODS(PDController, EditController)
  DEFINE_INTERFACES
  DEF_INTERFACE(IMidiMapping)
  END_DEFINE_INTERFACES(EditController)
  REFCOUNT_METHODS(EditController)

 private:
  PDEditor* activeEditor_ = nullptr;
  std::mutex scopeMutex_;
  std::vector<float> scopeData_;
  int32 skinIndex_ = 0;
};

}  // namespace Vst
}  // namespace Steinberg
