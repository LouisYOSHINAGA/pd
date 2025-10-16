#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"


namespace Steinberg {
namespace Vst {


class PDController: public EditController {
    public:
        static FUnknown* createInstance(void*);
        tresult PLUGIN_API initialize(FUnknown* context);
};


} }