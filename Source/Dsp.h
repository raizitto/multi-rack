#pragma once

#include <cmath>
#include <algorithm>
#include <vector>

namespace mr::dsp
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db)   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

/** One-pole smoothing coefficient for a time constant in milliseconds. */
inline float timeCoef (float ms, double fs)
{
    if (ms <= 0.0f) return 0.0f;
    return (float) std::exp (-1.0 / (ms * 0.001 * fs));
}

//==============================================================================
struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

namespace design
{
    inline double clampFreq (double f, double fs) { return std::clamp (f, 2.0, fs * 0.49); }

    inline Coeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        Coeffs c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0;
        c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }

    inline Coeffs peak (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * std::max (q, 0.05));
        return normalise (1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
    }

    inline Coeffs lowShelf (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * std::max (q, 0.05));
        const double sA = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) - (A - 1) * cw + sA),
                          2 * A * ((A - 1) - (A + 1) * cw),
                          A * ((A + 1) - (A - 1) * cw - sA),
                          (A + 1) + (A - 1) * cw + sA,
                          -2 * ((A - 1) + (A + 1) * cw),
                          (A + 1) + (A - 1) * cw - sA);
    }

    inline Coeffs highShelf (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * std::max (q, 0.05));
        const double sA = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) + (A - 1) * cw + sA),
                          -2 * A * ((A - 1) + (A + 1) * cw),
                          A * ((A + 1) + (A - 1) * cw - sA),
                          (A + 1) - (A - 1) * cw + sA,
                          2 * ((A - 1) - (A + 1) * cw),
                          (A + 1) - (A - 1) * cw - sA);
    }

    inline Coeffs lowpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }

    inline Coeffs highpass (double fs, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }

    inline Coeffs bandpass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * clampFreq (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise (alpha, 0, -alpha, 1 + alpha, -2 * cw, 1 - alpha);
    }

    inline Coeffs lowpass1 (double fs, double f)
    {
        const double K = std::tan (kPi * clampFreq (f, fs) / fs);
        Coeffs c;
        c.b0 = K / (1 + K); c.b1 = c.b0; c.a1 = (K - 1) / (K + 1);
        return c;
    }

    inline Coeffs highpass1 (double fs, double f)
    {
        const double K = std::tan (kPi * clampFreq (f, fs) / fs);
        Coeffs c;
        c.b0 = 1 / (1 + K); c.b1 = -c.b0; c.a1 = (K - 1) / (K + 1);
        return c;
    }

    inline Coeffs identity() { return {}; }
}

/** Stereo transposed direct form II biquad (double precision state). */
struct Biquad
{
    Coeffs c;
    double s1[2] { 0, 0 }, s2[2] { 0, 0 };

    void set (const Coeffs& n) { c = n; }
    void reset() { s1[0] = s1[1] = s2[0] = s2[1] = 0; }

    inline float process (float xf, int ch)
    {
        const double x = xf;
        const double y = c.b0 * x + s1[ch];
        s1[ch] = c.b1 * x - c.a1 * y + s2[ch];
        s2[ch] = c.b2 * x - c.a2 * y;
        return (float) y;
    }
};

struct DcBlocker
{
    float x1[2] { 0, 0 }, y1[2] { 0, 0 };
    float r = 0.999f;
    void setRate (double fs) { r = (float) (1.0 - 2.0 * kPi * 8.0 / fs); }
    void reset() { x1[0] = x1[1] = y1[0] = y1[1] = 0; }
    inline float process (float x, int ch)
    {
        const float y = x - x1[ch] + r * y1[ch];
        x1[ch] = x; y1[ch] = y;
        return y;
    }
};

/** Linear per-sample ramp used to de-zipper gains between parameter updates. */
struct Ramp
{
    float v = 1.0f, target = 1.0f, inc = 0.0f;
    int left = 0;
    bool init = false;

    void setTarget (float t, int n)
    {
        if (! init) { v = target = t; init = true; left = 0; return; }
        target = t;
        if (n <= 0 || t == v) { v = t; left = 0; return; }
        inc = (t - v) / (float) n;
        left = n;
    }
    void snap (float t) { v = target = t; left = 0; init = true; }
    inline float next()
    {
        if (left > 0) { v += inc; if (--left == 0) v = target; }
        return v;
    }
};

//==============================================================================
inline float satTanh (float x, float bias)  { return std::tanh (x + bias) - std::tanh (bias); }
inline float tanhSlope (float bias)         { const float t = std::tanh (bias); return 1.0f - t * t; }

inline float softClipCubic (float x)
{
    x = clampf (x, -1.5f, 1.5f);
    return x - (4.0f / 27.0f) * x * x * x;
}

/** Gain-computer: returns positive dB of gain reduction. */
inline float computeGR (float levelDb, float thresholdDb, float ratio, float kneeDb)
{
    const float over = levelDb - thresholdDb;
    const float slope = 1.0f - 1.0f / std::max (1.0f, ratio);

    if (kneeDb > 0.0f)
    {
        if (over <= -kneeDb * 0.5f) return 0.0f;
        if (over <   kneeDb * 0.5f)
        {
            const float t = over + kneeDb * 0.5f;
            return slope * t * t / (2.0f * kneeDb);
        }
        return slope * over;
    }
    return over > 0.0f ? slope * over : 0.0f;
}

/** Simple fractional delay line (linear interpolation). */
struct DelayLine
{
    std::vector<float> buf[2];
    int size = 0, w = 0;

    void allocate (int n) { size = std::max (4, n); buf[0].assign ((size_t) size, 0.0f); buf[1].assign ((size_t) size, 0.0f); w = 0; }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); w = 0; }

    inline void push (float l, float r)
    {
        buf[0][(size_t) w] = l; buf[1][(size_t) w] = r;
        if (++w >= size) w = 0;
    }
    /** Read 'd' samples behind the most recently written sample. */
    inline float read (int ch, float d) const
    {
        d = clampf (d, 0.0f, (float) (size - 2));
        float pos = (float) (w - 1) - d;
        while (pos < 0) pos += (float) size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float f = pos - (float) i0;
        return buf[ch][(size_t) i0] + f * (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]);
    }
};

} // namespace mr::dsp
