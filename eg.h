#pragma once

#include <array>
#include "pluginterfaces/vst/vsttypes.h"


namespace Steinberg {
namespace Vst {


class EgParam{
    private:
        static constexpr int N_EG_STEPS = 8;
        std::array<double, N_EG_STEPS> rates = {};
        std::array<double, N_EG_STEPS> levels = {};
    public:
        EgParam();
        virtual void setRate(int32, ParamValue);
        virtual void setLevel(int32, ParamValue);
};


} }