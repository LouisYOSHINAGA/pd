#pragma once

#include <memory>
#include "pluginterfaces/vst/vsttypes.h"
#include "const.h"
#include "eg.h"


namespace Steinberg{
namespace Vst{


class AbstractGenerator{
    protected:
        static constexpr double DCW_CORRECT_COEF = 0.95;
        double breakpoint;
        double slopeLeft;
        double slopeRight;
    public:
        AbstractGenerator();
        virtual void setDcw(double) = 0;
        virtual double getPhase(double) = 0;
        virtual double generate(double);
};

class SawToothGenerator: public AbstractGenerator{
    public:
        SawToothGenerator() = default;
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class SquareGenerator: public AbstractGenerator{
    public:
        SquareGenerator() = default;
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class PulseGenerator: public AbstractGenerator{
    public:
        PulseGenerator() = default;
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class DoubleSineGenerator: public AbstractGenerator{
    public:
        DoubleSineGenerator() = default;
        virtual void setDcw(double) override;
        virtual double getPhase(double) override;
};

class SawPulseGenerator: public AbstractGenerator{
    public:
        SawPulseGenerator() = default;
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
        ResonanceSawToothGenerator() = default;
        virtual double getEnvelope(double) override;
};

class ResonanceTriangleGenerator: public AbstractResonanceGenerator{
    public:
        ResonanceTriangleGenerator() = default;
        virtual double getEnvelope(double) override;
};

class ResonanceTrapezoidGenerator: public AbstractResonanceGenerator{
    public:
        ResonanceTrapezoidGenerator() = default;
        virtual double getEnvelope(double) override;
};


class PD{
    private:
        Waveform waveform;
        double phasetime;
        EG dcoEg;
        EG dcwEg;
        EG dcaEg;
        std::unique_ptr<AbstractGenerator> generator;
    public:
        PD();
        virtual void setWaveform(int8);
        virtual double generate(double, bool&);
        virtual void setupEg(void);
        virtual void setEgRate(int32, int32, ParamValue);
        virtual void setEgLevel(int32, int32, ParamValue);
        virtual void setEgSustainPoint(int32, int8);
        virtual void setEgEndPoint(int32, int8);
        virtual void restartEg(void);
        virtual void haltEg(void);
};


} }