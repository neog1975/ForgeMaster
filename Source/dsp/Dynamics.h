#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

inline float dbToGain (float db) { return std::exp (db * 0.11512925465f); }
inline float gainToDb (float g)  { return 8.68588963807f * std::log (std::max (g, 1.0e-9f)); }

// Feed-forward soft-knee compressor, smoothed in the dB domain.
// Linked mode uses one detector for both channels (L/R); unlinked runs
// independent detectors (used for Mid and Side).
class BandCompressor
{
public:
    void prepare (double fs) { sampleRate = fs; reset(); }
    void reset() { env[0] = env[1] = 0.0f; }

    void setTimes (float attackMs, float releaseMs)
    {
        attCoef = std::exp (-1.0f / (0.001f * attackMs * (float) sampleRate));
        relCoef = std::exp (-1.0f / (0.001f * releaseMs * (float) sampleRate));
    }

    // Returns the deepest gain reduction in dB (<= 0) over the block.
    float process (float* a, float* b, int n, float thresholdDb, float ratio, float makeupDb, bool linked)
    {
        const float makeup = dbToGain (makeupDb);
        float deepest = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            if (linked)
            {
                const float gr = smooth (0, computeGr (gainToDb (std::max (std::abs (a[i]), std::abs (b[i]))), thresholdDb, ratio));
                const float g = dbToGain (gr) * makeup;
                a[i] *= g; b[i] *= g;
                deepest = std::min (deepest, gr);
            }
            else
            {
                const float grA = smooth (0, computeGr (gainToDb (std::abs (a[i])), thresholdDb, ratio));
                const float grB = smooth (1, computeGr (gainToDb (std::abs (b[i])), thresholdDb, ratio));
                a[i] *= dbToGain (grA) * makeup;
                b[i] *= dbToGain (grB) * makeup;
                deepest = std::min (deepest, std::min (grA, grB));
            }
        }
        return deepest;
    }

private:
    static float computeGr (float xDb, float t, float r)
    {
        constexpr float knee = 6.0f;
        const float over = xDb - t;
        if (2.0f * over < -knee) return 0.0f;
        if (2.0f * std::abs (over) <= knee)
        {
            const float k = over + knee * 0.5f;
            return (1.0f / r - 1.0f) * k * k / (2.0f * knee);
        }
        return (1.0f / r - 1.0f) * over;
    }

    float smooth (int ch, float target)
    {
        float& e = env[ch];
        const float c = target < e ? attCoef : relCoef;
        e = c * e + (1.0f - c) * target;
        return e;
    }

    double sampleRate = 44100.0;
    float attCoef = 0.0f, relCoef = 0.0f;
    float env[2] {};
};

// Stereo-linked brickwall limiter with lookahead.
// Gain path: per-sample required gain -> sliding minimum over the lookahead
// window -> moving average of the same length -> release smoothing. The
// average reaches the required gain exactly when the delayed peak arrives,
// so the output never exceeds the ceiling (a final clip guards rounding).
class LookaheadLimiter
{
public:
    // lookahead must be >= 1 sample; the audio delay is lookahead - 1.
    void prepare (int lookahead)
    {
        window = std::max (1, lookahead);
        delayL.assign ((size_t) window, 0.0f);
        delayR.assign ((size_t) window, 0.0f);
        box.assign ((size_t) window, 1.0f);
        dqVal.assign ((size_t) window + 1, 1.0f);
        dqIdx.assign ((size_t) window + 1, 0);
        reset();
    }

    void reset()
    {
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0f);
        boxSum = (double) window;
        writePos = boxPos = 0;
        dqHead = dqTail = 0; dqCount = 0;
        t = 0; envelope = 1.0f;
    }

    int getDelay() const { return window - 1; }

    // driveStart/driveEnd are linear gains ramped across the block.
    // Returns the minimum applied gain (linear) and updates peakOut.
    float process (float* L, float* R, int n, float driveStart, float driveEnd,
                   float ceiling, float releaseCoef, float& peakOut)
    {
        const int cap = window + 1;
        float minGain = 1.0f;
        const float driveStep = n > 0 ? (driveEnd - driveStart) / (float) n : 0.0f;
        float drive = driveStart;

        for (int i = 0; i < n; ++i, drive += driveStep)
        {
            const float l = L[i] * drive, r = R[i] * drive;
            const float pk = std::max (std::abs (l), std::abs (r));
            const float req = pk > ceiling ? ceiling / pk : 1.0f;

            // sliding minimum (monotonic deque)
            while (dqCount > 0 && dqVal[(size_t) ((dqTail - 1 + cap) % cap)] >= req)
            {
                dqTail = (dqTail - 1 + cap) % cap; --dqCount;
            }
            dqVal[(size_t) dqTail] = req; dqIdx[(size_t) dqTail] = t;
            dqTail = (dqTail + 1) % cap; ++dqCount;
            while (dqIdx[(size_t) dqHead] <= t - window)
            {
                dqHead = (dqHead + 1) % cap; --dqCount;
            }
            const float held = dqVal[(size_t) dqHead];
            ++t;

            // moving average
            boxSum += held - box[(size_t) boxPos];
            box[(size_t) boxPos] = held;
            boxPos = (boxPos + 1) % window;
            const float avg = (float) (boxSum / window);

            envelope = avg > envelope ? avg + (envelope - avg) * releaseCoef : avg;

            // delay line
            delayL[(size_t) writePos] = l; delayR[(size_t) writePos] = r;
            writePos = (writePos + 1) % window;
            const float g = envelope;
            float ol = std::clamp (delayL[(size_t) writePos] * g, -ceiling, ceiling);
            float orr = std::clamp (delayR[(size_t) writePos] * g, -ceiling, ceiling);
            L[i] = ol; R[i] = orr;

            peakOut = std::max (peakOut, std::max (std::abs (ol), std::abs (orr)));
            minGain = std::min (minGain, g);
        }
        return minGain;
    }

private:
    int window = 1, writePos = 0, boxPos = 0;
    int dqHead = 0, dqTail = 0, dqCount = 0;
    long long t = 0;
    double boxSum = 1.0;
    float envelope = 1.0f;
    std::vector<float> delayL, delayR, box, dqVal;
    std::vector<long long> dqIdx;
};
