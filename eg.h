#pragma once

#include <array>
#include "pluginterfaces/vst/vsttypes.h"


namespace Steinberg {
namespace Vst {


class EgParam{
    private:
        static constexpr int8 EG_SUSTAIN_POINT_OFFSET = 0;
        static constexpr int8 EG_END_POINT_OFFSET = 2;
        static constexpr int8 EG_STEP_HALT = -1;
        static constexpr int8 EG_STEP_SUSTAIN = -2;
        static constexpr int8 N_EG_STEPS = 8;
        std::array<double, N_EG_STEPS+1> rates;
        std::array<double, N_EG_STEPS+1> levels;
        int8 sustainPoint;
        int8 endPoint;
        int8 step;
        double level;
        double dLevel;
        double target;
        void proceed(int8);
        void update(void);
    public:
        EgParam();
        virtual void setRate(int32, ParamValue);
        virtual void setLevel(int32, ParamValue);
        virtual void setSustainPoint(int8);
        virtual void setEndPoint(int8);
        virtual void setup(void);
        virtual double generate(void);
        virtual void restart(void);
};


} }