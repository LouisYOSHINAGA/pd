#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"


namespace Steinberg {
namespace Vst {


class PDController: public EditController,
                    public IMidiMapping {
    public:
        static FUnknown* createInstance(void*);
        tresult PLUGIN_API initialize(FUnknown* context);
        virtual tresult PLUGIN_API getMidiControllerAssignment(int32, int16, CtrlNumber, ParamID&);

        OBJ_METHODS(PDController, EditController)
            DEFINE_INTERFACES
                DEF_INTERFACE(IMidiMapping)
            END_DEFINE_INTERFACES(EditController)
            REFCOUNT_METHODS(EditController)
};


} }