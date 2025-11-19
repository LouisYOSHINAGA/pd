#pragma once

#include <memory>
#include "pluginterfaces/vst/vsttypes.h"
#include "const.h"
#include "eg.h"


namespace Steinberg {
namespace Vst {


class AbstractGenerator{
    protected:
        static constexpr double DCW_CORRECT_COEF = 0.95;
        double breakpoint;
        double slopeLeft;
        double slopeRight;
    public:
        virtual void setDcw(double) = 0;
        virtual double getPhase(double) = 0;
        virtual double generate(double);
};

class SawToothGenerator: public AbstractGenerator{
    public:
        SawToothGenerator() = delete;
        SawToothGenerator(double);
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class SquareGenerator: public AbstractGenerator{
    public:
        SquareGenerator() = delete;
        SquareGenerator(double);
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class PulseGenerator: public AbstractGenerator{
    public:
        PulseGenerator() = delete;
        PulseGenerator(double);
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class DoubleSineGenerator: public AbstractGenerator{
    public:
        DoubleSineGenerator() = delete;
        DoubleSineGenerator(double);
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class SawPulseGenerator: public AbstractGenerator{
    public:
        SawPulseGenerator() = delete;
        SawPulseGenerator(double);
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class AbstractResonanceGenerator: public AbstractGenerator{
    protected:
        static constexpr double MAX_FREQ_MULT = 14.0;
        double highFreqPhaseCoef;
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
        virtual double getEnvelope(double) = 0;
        virtual double generate(double) override;
};

class ResonanceSawToothGenerator: public AbstractResonanceGenerator{
    public:
        ResonanceSawToothGenerator() = delete;
        ResonanceSawToothGenerator(double);
        virtual double getEnvelope(double) override;
};

class ResonanceTriangleGenerator: public AbstractResonanceGenerator{
    public:
        ResonanceTriangleGenerator() = delete;
        ResonanceTriangleGenerator(double);
        virtual double getEnvelope(double) override;
};

class ResonanceTrapezoidGenerator: public AbstractResonanceGenerator{
    public:
        ResonanceTrapezoidGenerator() = delete;
        ResonanceTrapezoidGenerator(double);
        virtual double getEnvelope(double) override;
};


class PD{
    private:
        Waveform waveform;
        ParamValue dcw;
        double phasetime;
        std::unique_ptr<AbstractGenerator> generator;
        EgParam dcaEg;
    public:
        PD();
        virtual void setWaveform(int8);
        virtual void setDcw(ParamValue);
        virtual double generate(double);

        virtual void setupEg(void);
        virtual void setDcaRate(int32, ParamValue);
        virtual void setDcaLevel(int32, ParamValue);
        virtual void setDcaSustainPoint(int8);
        virtual void setDcaEndPoint(int8);
        virtual void restartEg(void);
};


} }