#pragma once

#include <algorithm>
#include <cmath>

// Split-band de-esser. A band-pass around the chosen frequency is compared with
// the level above 1 kHz (so kick and bass don't mask the comparison); when the
// sibilant band gets close to that level
// (an "s" or "sh"), only the content above ~0.7x the frequency is turned down.
// Output = input + (g - 1) * highpass(input), so with no reduction the signal
// passes bit-for-bit unchanged.
class DeEsser
{
public:
    void prepare (double fs)
    {
        sampleRate = fs;
        attack = coef (0.0005);
        release = coef (0.040);
        grAttack = coef (0.001);
        grRelease = coef (0.060);
        freq = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto* b : { &detect, &upper, &splitL, &splitR }) b->z1 = b->z2 = 0.0;
        envBand = envFull = 0.0;
        gr = 0.0;
    }

    // amount 0..1; returns the deepest reduction in dB (<= 0) for metering.
    float process (float* L, float* R, int n, float frequency, float amount)
    {
        if (amount <= 0.0f)
        {
            if (gr != 0.0) reset();
            return 0.0f;
        }
        if (frequency != freq)
            setFrequency (frequency);

        const double thresholdRel = -1.0 - 6.0 * amount;   // dB of sibilant band below the >1 kHz level
        const double maxReduction = 3.0 + 9.0 * amount;
        double deepest = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double l = L[i], r = R[i];
            const double m = 0.5 * (l + r);

            envBand = follow (envBand, std::abs (detect.process (m)));
            envFull = follow (envFull, std::abs (upper.process (m)));

            double target = 0.0;
            if (envFull > 1.0e-4)
            {
                const double rel = 20.0 * std::log10 ((envBand + 1.0e-12) / envFull);
                if (rel > thresholdRel)
                    target = -std::min ((rel - thresholdRel) * 3.0, maxReduction);
            }
            const double c = target < gr ? grAttack : grRelease;
            gr = c * gr + (1.0 - c) * target;
            deepest = std::min (deepest, gr);

            const double g = std::pow (10.0, gr / 20.0) - 1.0;
            L[i] = (float) (l + g * splitL.process (l));
            R[i] = (float) (r + g * splitR.process (r));
        }
        return (float) deepest;
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    double coef (double seconds) const { return std::exp (-1.0 / (seconds * sampleRate)); }

    double follow (double env, double x) const
    {
        const double c = x > env ? attack : release;
        return c * env + (1.0 - c) * x;
    }

    void setFrequency (float f)
    {
        freq = f;
        constexpr double pi = 3.14159265358979323846;
        const double nyq = sampleRate * 0.45;

        // detection band-pass (constant 0 dB peak), Q 1
        {
            const double w = 2.0 * pi * std::min ((double) f, nyq) / sampleRate;
            const double alpha = std::sin (w) / 2.0, a0 = 1.0 + alpha;
            detect.b0 = alpha / a0; detect.b1 = 0.0; detect.b2 = -alpha / a0;
            detect.a1 = -2.0 * std::cos (w) / a0; detect.a2 = (1.0 - alpha) / a0;
        }
        // reference level: everything above 1 kHz
        {
            const double w = 2.0 * pi * 1000.0 / sampleRate;
            const double alpha = std::sin (w) / (2.0 * 0.7071), a0 = 1.0 + alpha, c = std::cos (w);
            upper.b0 = (1.0 + c) / 2.0 / a0; upper.b1 = -(1.0 + c) / a0; upper.b2 = upper.b0;
            upper.a1 = -2.0 * c / a0; upper.a2 = (1.0 - alpha) / a0;
        }
        // split high-pass (Butterworth) a little below the sibilant band
        for (auto* b : { &splitL, &splitR })
        {
            const double w = 2.0 * pi * std::min (0.7 * f, nyq) / sampleRate;
            const double alpha = std::sin (w) / (2.0 * 0.7071), a0 = 1.0 + alpha, c = std::cos (w);
            b->b0 = (1.0 + c) / 2.0 / a0; b->b1 = -(1.0 + c) / a0; b->b2 = b->b0;
            b->a1 = -2.0 * c / a0; b->a2 = (1.0 - alpha) / a0;
        }
    }

    double sampleRate = 44100.0;
    float freq = -1.0f;
    double attack = 0, release = 0, grAttack = 0, grRelease = 0;
    double envBand = 0, envFull = 0, gr = 0;
    Biquad detect, upper, splitL, splitR;
};
