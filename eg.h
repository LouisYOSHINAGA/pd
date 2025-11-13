#pragma once

#include <array>
#include "pluginterfaces/vst/vsttypes.h"


namespace Steinberg {
namespace Vst {


class EgParam{
    private:
        static constexpr int8 EG_STEP_HALT = -1;
        static constexpr int8 N_EG_STEPS = 8;
        std::array<double, N_EG_STEPS+1> rates;
        std::array<double, N_EG_STEPS+1> levels;
        int8 step;
        double level;
        double dLevel;
        void proceed(int8);
        void halt(void);
        void update(void);
    public:
        EgParam();
        virtual void setRate(int32, ParamValue);
        virtual void setLevel(int32, ParamValue);
        virtual void setup(void);
        virtual double generate(void);
};


} }