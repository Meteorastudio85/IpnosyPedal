/*
    Ipnosy - Meteora Plugins
    Copyright (c) 2026 Meteora Plugins. Tutti i diritti riservati.

    Stadio valvolare preso da Marley (Meteora Plugins): Airwindows Tube2 in
    oversampling a fase lineare, DC blocker e autogain K-pesato.
    Qui e' in versione mono e con l'accensione "a riscaldamento".
*/

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include "AutoGain.h"
#include "LinearPhaseOversampler.h"
#include "Tube2Stage.h"

class TubeEngine
{
public:
    static constexpr int kSubBlock = 16;

    static constexpr double kWarmUpSeconds   = 1.6; // tempo per scaldarsi del tutto
    static constexpr double kCoolDownSeconds = 0.9; // tempo per raffreddarsi

    std::atomic<float> warm { 0.0f };       // 0..1: quanto la valvola e' calda (segue il suono)
    std::atomic<float> levelDb { -100.0f }; // livello RMS che entra nella valvola (dopo il Drive)

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        osFactor = (sampleRate >= 88200.0 ? 2 : 4);
        os.setFactor (osFactor);
        tube.prepare (sampleRate * osFactor);

        latency = os.getLatency();
        delay.assign ((size_t) latency, 0.0);
        pos = 0;

        dcR = 1.0 - (2.0 * 3.14159265358979323846 * 2.0 / sampleRate);
        dc = {};

        autoGain.prepare (sampleRate, sampleRate * osFactor, 1);

        smoothCoef = std::exp (-1.0 / (0.03 * sampleRate));
        warmUpStep   = 1.0 / (kWarmUpSeconds * sampleRate);
        coolDownStep = 1.0 / (kCoolDownSeconds * sampleRate);
        snap = true;
        levelMs = 0.0;
    }

    int getLatency() const { return latency; }

    // io: segnale mono (in/out). driveTarget 0..1, tubeOn = valvola accesa (pomello oltre ore 15).
    void process (float* io, int numSamples, double driveTarget, bool tubeOn)
    {
        if (numSamples <= 0)
            return;

        if (snap)
        {
            drive = driveTarget;
            snap = false;
        }

        double powSum = 0.0;

        for (int start = 0; start < numSamples; start += kSubBlock)
        {
            const int n = std::min (kSubBlock, numSamples - start);

            const double c = std::pow (smoothCoef, (double) n);
            drive = c * drive + (1.0 - c) * driveTarget;

            const auto tp = DriveVoicing::params (drive);
            const double inGain2 = tp.inGain * tp.inGain;

            const double trim  = autoGain.getTrim();
            const double ag0   = autoGain.getStaticGain (drive);
            const double ag1   = autoGain.updateStaticGain (drive, n);
            const double agStep = (ag1 - ag0) / (double) n;
            double ag = ag0;

            for (int i = start; i < start + n; ++i)
            {
                const double x = (double) io[i];
                autoGain.detect (x * x);
                ag += agStep;
                powSum += x * x * inGain2;

                double osb[LinearPhaseOversampler4x::kMaxFactor];
                os.up (x, osb);
                for (int k = 0; k < osFactor; ++k)
                    osb[k] = tube.process (osb[k], tp);
                double wet = dcBlock (os.down (osb));

                const double delayed = delay[(size_t) pos];
                delay[(size_t) pos] = x;
                pos = (pos + 1 == latency) ? 0 : pos + 1;

                wet *= ag;
                autoGain.measureLoudness (0, delayed, wet);
                wet *= trim;

                // riscaldamento / raffreddamento lineare, poi curva morbida
                if (tubeOn) warmState = std::min (1.0, warmState + warmUpStep);
                else        warmState = std::max (0.0, warmState - coolDownStep);
                const double a = warmState * warmState * (3.0 - 2.0 * warmState);

                io[i] = (float) (delayed + a * (wet - delayed));
            }

            autoGain.updateTrim (n);
        }

        const double cAvg = std::exp (-(double) numSamples / (sr * 0.3));
        levelMs = cAvg * levelMs + (1.0 - cAvg) * (powSum / (double) numSamples);
        levelDb.store ((float) (10.0 * std::log10 (levelMs + 1.0e-12)));
        warm.store ((float) warmState);
    }

private:
    struct DcState { double x1 = 0.0, y1 = 0.0; };

    inline double dcBlock (double x)
    {
        const double y = x - dc.x1 + dcR * dc.y1;
        dc.x1 = x;
        dc.y1 = y;
        return y;
    }

    double sr = 48000.0;
    int osFactor = 4;
    int latency = LinearPhaseOversampler4x::kLatency4x;

    LinearPhaseOversampler4x os;
    Tube2Stage tube;
    AutoGain autoGain;

    std::vector<double> delay;
    int pos = 0;

    double dcR = 0.9997;
    DcState dc;

    double smoothCoef = 0.999;
    double drive = 0.5;
    bool snap = true;

    double warmState = 0.0, warmUpStep = 0.0, coolDownStep = 0.0;
    double levelMs = 0.0;
};
