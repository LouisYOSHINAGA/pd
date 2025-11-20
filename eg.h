#pragma once

#include <array>
#include "pluginterfaces/vst/vsttypes.h"


namespace Steinberg {
namespace Vst {


class AbstractEG{
    protected:
        static constexpr int8 N_EG_STEPS = 8;
        static constexpr int8 EG_STEP_HALT = -1;
        static constexpr int8 EG_STEP_SUSTAIN = -2;
        static constexpr int8 EG_SUSTAIN_OFF = N_EG_STEPS + 1;
        static constexpr int8 EG_SUSTAIN_POINT_OFFSET = 0;
        static constexpr int8 EG_END_POINT_OFFSET = 2;
        std::array<double, N_EG_STEPS+1> rates;
        std::array<double, N_EG_STEPS+1> levels;
        int8 sustainPoint;
        int8 endPoint;
        int8 step;
        double level;
        double dLevel;
        double target;
        virtual void proceed(int8) = 0;
        virtual void update(void);
    public:
        AbstractEG();
        virtual void setRate(int32, ParamValue);
        virtual void setLevel(int32, ParamValue);
        virtual void setSustainPoint(int8);
        virtual void setEndPoint(int8);
        virtual void setup(void);
        virtual double generate(void);
        virtual void restart(void);
};


class ZeroEndEG: public AbstractEG{
    protected:
        virtual void proceed(int8) override;
    public:
        ZeroEndEG();
};


} }