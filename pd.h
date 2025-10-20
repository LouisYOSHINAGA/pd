#pragma once

#include <memory>
#include "pluginterfaces/vst/vsttypes.h"
#include "const.h"


namespace Steinberg {
namespace Vst {


class AbstractPhaseGenerator{
    public:
        virtual void setDcw(double) = 0;
        virtual double getPhase(double) = 0;
};

class SawToothPhaseGenerator: public AbstractPhaseGenerator{
    private:
        double breakpoint;
        double slopeLeft;
        double slopeRight;
    public:
        SawToothPhaseGenerator();
        SawToothPhaseGenerator(double);
        virtual void setDcw(double);
        virtual double getPhase(double);
};

// class SquarePhaseGenerator: public AbstractPhaseGenerator{
//     private:
//         double breakpoint;
//         double slopeLeft;
//         double slopeRight;
//     public:
//         SquarePhaseGenerator();
//         SquarePhaseGenerator(double);
//         virtual void setDcw(double);
//         virtual double getPhase(double);
// };

// class PulsePhaseGenerator: public AbstractPhaseGenerator{
//     private:
//         double breakpoint;
//         double slopeLeft;
//         double slopeRight;
//     public:
//         PulsePhaseGenerator();
//         PulsePhaseGenerator(double);
//         virtual void setDcw(double);
//         virtual double getPhase(double);
// };

// class DoubleSinePhaseGenerator: public AbstractPhaseGenerator{
//     private:
//         // TODO: impl
//     public:
//         DoubleSinePhaseGenerator(double);
//         virtual void setDcw(double);
//         virtual double getPhase(double);
// };

// class SawPlusePhaseGenerator: public AbstractPhaseGenerator{
//     private:
//         // TODO: impl
//     public:
//         SawPlusePhaseGenerator(double);
//         virtual void setDcw(double);
//         virtual double getPhase(double);
// };

// class ResonanceSawToothPhaseGenerator: public AbstractPhaseGenerator{
//     private:
//         // TODO: impl
//     public:
//         ResonanceSawToothPhaseGenerator(double);
//         virtual void setDcw(double);
//         virtual double getPhase(double);
// };

// class ResonanceTrianglePhaseGenerator: public AbstractPhaseGenerator{
//     private:
//         // TODO: impl
//     public:
//         ResonanceTrianglePhaseGenerator(double);
//         virtual void setDcw(double);
//         virtual double getPhase(double);
// };

// class ResonanceTrapezoidPhaseGenerator: public AbstractPhaseGenerator{
//     private:
//         // TODO: impl
//     public:
//         ResonanceTrapezoidPhaseGenerator(double);
//         virtual void setDcw(double);
//         virtual double getPhase(double);
// };


class PD{
    private:
        WaveformType waveform;
        ParamValue dcw;
        double phasetime;

        SawToothPhaseGenerator sawToothPhaseGenerator;
        // SquarePhaseGenerator squarePhaseGenerator;
        // PulsePhaseGenerator pulsePhaseGenerator;
        // DoubleSinePhaseGenerator doubleSinePhaseGenerator;
        // SawPlusePhaseGenerator sawPulsePhaseGenerator;
        // ResonanceSawToothPhaseGenerator resonanceSawToothPhaseGenerator;
        // ResonanceTrianglePhaseGenerator resonanceTrianglePhaseGenerator;
        // ResonanceTrapezoidPhaseGenerator resonanceTrapezoidPhaseGenerator;
        // AbstractPhaseGenerator* phaseGenerator;
        std::unique_ptr<AbstractPhaseGenerator> phaseGenerator;
    public:
        PD();
        virtual void setWaveform(WaveformType);
        virtual void setDcw(void);
        virtual void setDcw(ParamValue);
        virtual double generate(double);
};


} }