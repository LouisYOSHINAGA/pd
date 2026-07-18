#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

namespace Steinberg {
namespace Vst {

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
  tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                 CtrlNumber midiControllerNumber,
                                                 ParamID& id) override;

  OBJ_METHODS(PDController, EditController)
  DEFINE_INTERFACES
  DEF_INTERFACE(IMidiMapping)
  END_DEFINE_INTERFACES(EditController)
  REFCOUNT_METHODS(EditController)
};

}  // namespace Vst
}  // namespace Steinberg
