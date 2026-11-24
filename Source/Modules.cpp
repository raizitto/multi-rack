#include "Modules.h"
#include <cmath>

namespace mr
{
using namespace dsp;

namespace
{
//==============================================================================
// Shared helpers
//==============================================================================
struct VuFollower
{
    float state = 0.0f;
    void reset() { state = 0.0f; }
    inline void add (float l, float r, float coef)
    {
        const float e = 0.5f * (l * l + r * r);
        state = coef * state + (1.0f - coef) * e;
    }
    float db() const { return gainToDb (std::sqrt (state)); }
};

struct PeakHold
{
    float v = 0.0f;
    inline void add (float l, float r) { v = std::max (v, std::max (std::abs (l), std::abs (r))); }
    void publish (std::atomic<float>& m, double fs, int n)
    {
        // falling peak meter in dB
        const float cur = m.load();
        const float fall = 20.0f * (float) n / (float) fs; // 20 dB per second
        const float db = gainToDb (v);
        m.store (std::max (db, cur - fall));
        v = 0.0f;
    }
};

/** Smoothed gain-reduction envelope operating in the dB domain. */
struct GrEnvelope
{
    float env = 0.0f;
    void reset() { env = 0.0f; }
    inline float process (float target, float atk, float rel)
    {
        const float c = target > env ? atk : rel;
        env = c * env + (1.0f - c) * target;
        return env;
    }
};

//==============================================================================
// 1: Analog Channel
//==============================================================================
class ChannelModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    struct Flavour
    {
        float driveDb, bias, hardness;
        float bumpDb, bumpHz;
        float shelfDb, shelfHz;
        float lpHz;
        float lowEmphasisDb;
    };

    static const Flavour& flavour (int i)
    {
        static const Flavour f[] = {
            { 18.0f, 0.00f, 0.00f, 0.0f,  60.0f,  0.0f, 10000.0f, 40000.0f, 0.0f }, // Clean
            { 24.0f, 0.10f, 0.00f, 0.7f,  50.0f, -0.4f, 12000.0f, 26000.0f, 6.0f }, // Iron
            { 20.0f, 0.02f, 0.50f, 0.0f,  60.0f,  0.7f, 10000.0f, 40000.0f, 0.0f }, // Glass
            { 22.0f, 0.28f, 0.00f, 0.3f,  80.0f, -0.6f,  8000.0f, 21000.0f, 0.0f }, // Tube
            { 26.0f, 0.12f, 0.70f, 0.9f, 100.0f,  0.5f,  3500.0f, 30000.0f, 0.0f }, // Punch
            { 22.0f, 0.16f, 0.00f, 0.5f,  60.0f,  0.9f, 14000.0f, 30000.0f, 3.0f }, // Silk
        };
        return f[juce::jlimit (0, 5, i)];
    }

    void reset() override
    {
        for (auto* b : { &hp1, &hp2, &lfPre, &lfPost, &bump, &shelf, &lp, &tiltLo, &tiltHi }) b->reset();
        dc.reset(); vu.reset(); inG.init = outG.init = false;
    }

    void onRateChanged() override { dc.setRate (fs); }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        const float inDb = real (p, 0), drive = real (p, 1);
        const int ch = idx (p, 2), hpIdx = idx (p, 3);
        const float tone = real (p, 4), outDb = real (p, 5);
        const auto& f = flavour (ch);

        if (dirty || ch != lastChar || hpIdx != lastHp || tone != lastTone)
        {
            static const float hpFreqs[] = { 0.0f, 20.0f, 40.0f, 80.0f, 120.0f };
            hpOn = hpIdx > 0;
            if (hpOn)
            {
                hp1.set (design::highpass1 (fs, hpFreqs[hpIdx]));
                hp2.set (design::highpass (fs, hpFreqs[hpIdx], 1.0));
            }
            lfPre.set  (design::lowShelf (fs, 140.0, 0.6, f.lowEmphasisDb));
            lfPost.set (design::lowShelf (fs, 140.0, 0.6, -f.lowEmphasisDb));
            bump.set   (design::peak (fs, f.bumpHz, 0.9, f.bumpDb));
            shelf.set  (design::highShelf (fs, f.shelfHz, 0.7, f.shelfDb));
            lp.set     (f.lpHz < fs * 0.45 ? design::lowpass (fs, f.lpHz, 0.6) : design::identity());
            tiltLo.set (design::lowShelf (fs, 300.0, 0.6, -tone));
            tiltHi.set (design::highShelf (fs, 3000.0, 0.6, tone));
            lastChar = ch; lastHp = hpIdx; lastTone = tone; dirty = false;
        }

        const float g = dbToGain (-6.0f + drive * 0.1f * f.driveDb);
        const float softSlope = tanhSlope (f.bias);
        const float hardBias = clampf (f.bias, -1.5f, 1.5f);
        const float hardBase = softClipCubic (hardBias);
        const float hardSlope = 1.0f - (12.0f / 27.0f) * hardBias * hardBias;
        const float norm = 1.0f / (g * lerp (softSlope, hardSlope, f.hardness));
        const bool lfe = f.lowEmphasisDb != 0.0f;

        inG.setTarget (dbToGain (inDb), n);
        outG.setTarget (dbToGain (outDb), n);
        const float vuC = timeCoef (300.0f, fs);

        for (int i = 0; i < n; ++i)
        {
            const float gi = inG.next(), go = outG.next();
            float* io[2] = { L + i, R + i };

            for (int c = 0; c < 2; ++c)
            {
                float x = *io[c] * gi;
                if (hpOn) x = hp2.process (hp1.process (x, c), c);
                if (lfe)  x = lfPre.process (x, c);

                const float u = x * g;
                float s = satTanh (u, f.bias);
                if (f.hardness > 0.0f)
                    s = lerp (s, softClipCubic (u + hardBias) - hardBase, f.hardness);

                float y = dc.process (s * norm, c);
                if (lfe) y = lfPost.process (y, c);
                y = bump.process (y, c);
                y = shelf.process (y, c);
                y = lp.process (y, c);
                y = tiltHi.process (tiltLo.process (y, c), c);
                *io[c] = y * go;
            }
            vu.add (L[i], R[i], vuC);
        }
        meter.store (vu.db());
    }

private:
    Biquad hp1, hp2, lfPre, lfPost, bump, shelf, lp, tiltLo, tiltHi;
    DcBlocker dc;
    VuFollower vu;
    Ramp inG, outG;
    bool hpOn = false;
    int lastChar = -1, lastHp = -1;
    float lastTone = 1.0e9f;
};

//==============================================================================
// 2: Quad EQ
//==============================================================================
class QuadEqModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { for (auto& b : bands) b.reset(); outG.init = false; }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        bool changed = dirty;
        for (int i = 0; i < 15; ++i)
            if (p[i] != last[i]) { changed = true; last[i] = p[i]; }

        if (changed)
        {
            const bool prop = idx (p, 14) == 1;
            auto q = [prop] (float baseQ, float gainDb)
            {
                return prop ? baseQ * (0.4f + 1.2f * std::abs (gainDb) / 15.0f) : baseQ;
            };

            const float hfG = real (p, 0), hfF = real (p, 1);
            const float hmG = real (p, 3), hmF = real (p, 4), hmQ = real (p, 5);
            const float lmG = real (p, 6), lmF = real (p, 7), lmQ = real (p, 8);
            const float lfG = real (p, 9), lfF = real (p, 10);
            const float hpF = real (p, 12), lpF = real (p, 13);

            bands[0].set (isOn (p, 2) ? design::peak (fs, hfF, q (0.9f, hfG), hfG)
                                       : design::highShelf (fs, hfF, 0.7, hfG));
            bands[1].set (design::peak (fs, hmF, q (hmQ, hmG), hmG));
            bands[2].set (design::peak (fs, lmF, q (lmQ, lmG), lmG));
            bands[3].set (isOn (p, 11) ? design::peak (fs, lfF, q (0.9f, lfG), lfG)
                                        : design::lowShelf (fs, lfF, 0.7, lfG));

            hpOn = ! info.params[12].isOff (hpF);
            lpOn = ! info.params[13].isOff (lpF) && lpF < fs * 0.45;
            if (hpOn)
            {
                bands[4].set (design::highpass1 (fs, hpF));
                bands[5].set (design::highpass (fs, hpF, 1.0));
            }
            if (lpOn)
                bands[6].set (design::lowpass (fs, lpF, 0.7071));

            gainsZero[0] = std::abs (hfG) < 0.01f;
            gainsZero[1] = std::abs (hmG) < 0.01f;
            gainsZero[2] = std::abs (lmG) < 0.01f;
            gainsZero[3] = std::abs (lfG) < 0.01f;
            dirty = false;
        }

        outG.setTarget (dbToGain (real (p, 15)), n);

        for (int i = 0; i < n; ++i)
        {
            const float go = outG.next();
            float* io[2] = { L + i, R + i };
            for (int c = 0; c < 2; ++c)
            {
                float x = *io[c];
                if (hpOn) x = bands[5].process (bands[4].process (x, c), c);
                for (int b = 3; b >= 0; --b)
                    if (! gainsZero[b]) x = bands[b].process (x, c);
                if (lpOn) x = bands[6].process (x, c);
                *io[c] = x * go;
            }
        }
    }

private:
    Biquad bands[7];
    bool hpOn = false, lpOn = false, gainsZero[4] { true, true, true, true };
    float last[16] { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    Ramp outG;
};

//==============================================================================
// 3: Heritage EQ
//==============================================================================
class HeritageEqModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { for (auto& b : f) b.reset(); dc.reset(); outG.init = false; }
    void onRateChanged() override { dc.setRate (fs); }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        bool changed = dirty;
        for (int i = 0; i < 9; ++i)
            if (p[i] != last[i]) { changed = true; last[i] = p[i]; }

        if (changed)
        {
            static const float hfF[] = { 10000, 12000, 16000 };
            static const float hmF[] = { 1600, 3200, 4800, 7200 };
            static const float lmF[] = { 360, 700, 1600, 3200 };
            static const float lfF[] = { 35, 60, 110, 220 };
            static const float hpF[] = { 0, 50, 80, 160, 300 };

            f[0].set (design::highShelf (fs, hfF[idx (p, 1)], 0.7, real (p, 0)));
            f[1].set (design::peak (fs, hmF[idx (p, 3)], 0.8, real (p, 2)));
            f[2].set (design::peak (fs, lmF[idx (p, 5)], 0.8, real (p, 4)));
            f[3].set (design::lowShelf (fs, lfF[idx (p, 7)], 1.0, real (p, 6)));
            hpIdx = idx (p, 8);
            if (hpIdx > 0)
            {
                f[4].set (design::highpass1 (fs, hpF[hpIdx]));
                f[5].set (design::highpass (fs, hpF[hpIdx], 1.0));
            }
            dirty = false;
        }

        const bool driveOn = isOn (p, 10);
        const float g = dbToGain (real (p, 9) - 6.0f);
        const float bias = 0.12f;
        const float norm = 1.0f / (g * tanhSlope (bias));
        outG.setTarget (dbToGain (real (p, 11)), n);

        for (int i = 0; i < n; ++i)
        {
            const float go = outG.next();
            float* io[2] = { L + i, R + i };
            for (int c = 0; c < 2; ++c)
            {
                float x = *io[c];
                if (hpIdx > 0) x = f[5].process (f[4].process (x, c), c);
                x = f[3].process (x, c);
                x = f[2].process (x, c);
                x = f[1].process (x, c);
                x = f[0].process (x, c);
                if (driveOn)
                    x = dc.process (satTanh (x * g, bias) * norm, c);
                *io[c] = x * go;
            }
        }
    }

private:
    Biquad f[6];
    DcBlocker dc;
    int hpIdx = 0;
    float last[9] { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    Ramp outG;
};

//==============================================================================
// 4: Program EQ (passive tube style)
//==============================================================================
class ProgramEqModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { for (auto& b : f) b.reset(); dc.reset(); outG.init = false; }
    void onRateChanged() override { dc.setRate (fs); }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        bool changed = dirty;
        for (int i = 0; i < 8; ++i)
            if (p[i] != last[i]) { changed = true; last[i] = p[i]; }

        if (changed)
        {
            static const float lowF[]  = { 20, 30, 60, 100 };
            static const float highF[] = { 3000, 4000, 5000, 8000, 10000, 12000, 16000 };
            static const float attF[]  = { 5000, 10000, 20000 };

            const float lf = lowF[idx (p, 0)];
            f[0].set (design::lowShelf (fs, lf, 0.9, real (p, 1) * 1.35));        // low boost
            f[1].set (design::lowShelf (fs, lf * 1.4, 0.45, -real (p, 2) * 1.75)); // low atten

            const float bw = real (p, 5) / 10.0f;                       // 0 = sharp, 1 = broad
            const double hq = 2.5 * std::pow (0.35 / 2.5, (double) bw);
            f[2].set (design::peak (fs, highF[idx (p, 3)], hq, real (p, 4) * 1.6));
            f[3].set (design::highShelf (fs, attF[idx (p, 6)], 0.6, -real (p, 7) * 1.6));
            dirty = false;
        }

        const float drive = real (p, 8);
        const float g = dbToGain (-12.0f + drive * 1.8f);
        const float bias = 0.2f;
        const float norm = 1.0f / (g * tanhSlope (bias));
        outG.setTarget (dbToGain (real (p, 9)), n);

        for (int i = 0; i < n; ++i)
        {
            const float go = outG.next();
            float* io[2] = { L + i, R + i };
            for (int c = 0; c < 2; ++c)
            {
                float x = *io[c];
                x = f[0].process (x, c);
                x = f[1].process (x, c);
                x = f[2].process (x, c);
                x = f[3].process (x, c);
                x = dc.process (satTanh (x * g, bias) * norm, c);
                *io[c] = x * go;
            }
        }
    }

private:
    Biquad f[4];
    DcBlocker dc;
    float last[8] { -1, -1, -1, -1, -1, -1, -1, -1 };
    Ramp outG;
};

//==============================================================================
// 5: Bus Comp (VCA)
//==============================================================================
class BusCompModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { gr.reset(); sc.reset(); avg = 0; mkG.init = mixR.init = false; }

    void process (float* L, float* R, const float* eL, const float* eR, int n, const float* p) override
    {
        static const float ratios[]  = { 1.5f, 2.0f, 4.0f, 10.0f };
        static const float attacks[] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
        static const float rels[]    = { 100.0f, 300.0f, 600.0f, 1200.0f };

        const float thr = real (p, 0);
        const float ratio = ratios[idx (p, 1)];
        const float atk = timeCoef (attacks[idx (p, 2)], fs);
        const int relIdx = idx (p, 3);
        const float hpF = real (p, 5);
        const bool hpOn = ! info.params[5].isOff (hpF);
        const bool ext = isOn (p, 7) && eL != nullptr;

        if (dirty || hpF != lastHp)
        {
            sc.set (design::highpass (fs, hpF, 0.7071));
            lastHp = hpF; dirty = false;
        }

        float relMs = relIdx < 4 ? rels[relIdx] : 0.0f;
        if (relIdx == 4) // auto: program dependent
            relMs = 100.0f + 1100.0f * clampf (avg / 10.0f, 0.0f, 1.0f);
        const float rel = timeCoef (relMs, fs);
        const float avgC = timeCoef (800.0f, fs);

        mkG.setTarget (dbToGain (real (p, 4)), n);
        mixR.setTarget (real (p, 6) * 0.01f, n);

        for (int i = 0; i < n; ++i)
        {
            float kl = ext ? eL[i] : L[i];
            float kr = ext ? eR[i] : R[i];
            if (hpOn) { kl = sc.process (kl, 0); kr = sc.process (kr, 1); }

            const float lvl = gainToDb (std::max (std::abs (kl), std::abs (kr)));
            const float e = gr.process (computeGR (lvl, thr, ratio, 6.0f), atk, rel);
            avg = avgC * avg + (1.0f - avgC) * e;

            const float g = dbToGain (-e) * mkG.next();
            const float mix = mixR.next();
            L[i] = L[i] * (1.0f - mix) + L[i] * g * mix;
            R[i] = R[i] * (1.0f - mix) + R[i] * g * mix;
        }
        meter.store (gr.env);
    }

private:
    GrEnvelope gr;
    Biquad sc;
    float avg = 0.0f, lastHp = -1.0f;
    Ramp mkG, mixR;
};

//==============================================================================
// 6: Stress Comp (FET)
//==============================================================================
class StressCompModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { gr.reset(); det.reset(); dc.reset(); inG.init = outG.init = mixR.init = false; }

    void onRateChanged() override
    {
        det.set (design::highpass (fs, 100.0, 0.7071));
        dc.setRate (fs);
    }

    void process (float* L, float* R, const float* eL, const float* eR, int n, const float* p) override
    {
        static const float ratios[] = { 2.0f, 4.0f, 8.0f, 12.0f, 20.0f, 50.0f };
        const int rIdx = idx (p, 4);
        const bool nuke = rIdx == 5;
        const float ratio = ratios[rIdx];
        const float thr = nuke ? -24.0f : -20.0f;
        const float knee = nuke ? 0.0f : 3.0f;
        const float atk = timeCoef (real (p, 2) * (nuke ? 0.5f : 1.0f), fs);
        const float rel = timeCoef (real (p, 3) * (nuke ? 1.4f : 1.0f), fs);
        const bool detHp = isOn (p, 6);
        const bool ext = isOn (p, 8) && eL != nullptr;

        const float dist = real (p, 7) / 10.0f + (nuke ? 0.25f : 0.0f);
        const float dDrive = 1.0f + 3.0f * dist;
        const float dBias = 0.1f * dist;
        const float dNorm = 1.0f / (dDrive * tanhSlope (dBias));

        inG.setTarget (dbToGain (real (p, 0)), n);
        outG.setTarget (dbToGain (real (p, 1)), n);
        mixR.setTarget (real (p, 5) * 0.01f, n);

        for (int i = 0; i < n; ++i)
        {
            const float gi = inG.next(), go = outG.next(), mix = mixR.next();
            const float xl = L[i] * gi, xr = R[i] * gi;

            float kl = ext ? eL[i] * gi : xl;
            float kr = ext ? eR[i] * gi : xr;
            if (detHp) { kl = det.process (kl, 0); kr = det.process (kr, 1); }

            const float lvl = gainToDb (std::max (std::abs (kl), std::abs (kr)));
            const float e = gr.process (computeGR (lvl, thr, ratio, knee), atk, rel);
            const float g = dbToGain (-e);

            float yl = xl * g, yr = xr * g;
            if (dist > 0.0f)
            {
                yl = dc.process (satTanh (yl * dDrive, dBias) * dNorm, 0);
                yr = dc.process (satTanh (yr * dDrive, dBias) * dNorm, 1);
            }

            L[i] = (L[i] * (1.0f - mix) + yl * mix * go);
            R[i] = (R[i] * (1.0f - mix) + yr * mix * go);
        }
        meter.store (gr.env);
    }

private:
    GrEnvelope gr;
    Biquad det;
    DcBlocker dc;
    Ramp inG, outG, mixR;
};

//==============================================================================
// 7: Opto Comp
//==============================================================================
class OptoCompModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { env = mem = rms = 0; emph.reset(); dc.reset(); gainR.init = mixR.init = false; }
    void onRateChanged() override { dc.setRate (fs); }

    void process (float* L, float* R, const float* eL, const float* eR, int n, const float* p) override
    {
        const float pr = real (p, 0);
        const bool limit = idx (p, 2) == 1;
        const float emphasis = real (p, 3);
        const float color = real (p, 4);
        const bool ext = isOn (p, 6) && eL != nullptr;

        if (dirty || emphasis != lastEmph)
        {
            emph.set (design::highShelf (fs, 2500.0, 0.6, emphasis * 0.12));
            lastEmph = emphasis; dirty = false;
        }

        const float thr = -pr * 0.45f;
        const float ratio = limit ? 10.0f : 3.0f;
        const float knee = limit ? 4.0f : 10.0f;
        const float atk = timeCoef (10.0f, fs);
        const float relFast = timeCoef (80.0f, fs);
        const float memCharge = timeCoef (1500.0f, fs);
        const float memRelease = timeCoef (3000.0f, fs);
        const float rmsC = timeCoef (5.0f, fs);

        const float tg = dbToGain (-12.0f + color * 2.0f);
        const float bias = 0.15f;
        const float tNorm = 1.0f / (tg * tanhSlope (bias));

        gainR.setTarget (dbToGain (real (p, 1)), n);
        mixR.setTarget (real (p, 5) * 0.01f, n);

        for (int i = 0; i < n; ++i)
        {
            float kl = ext ? eL[i] : L[i];
            float kr = ext ? eR[i] : R[i];
            if (emphasis > 0.0f) { kl = emph.process (kl, 0); kr = emph.process (kr, 1); }

            rms = rmsC * rms + (1.0f - rmsC) * 0.5f * (kl * kl + kr * kr);
            const float lvl = 10.0f * std::log10 (std::max (rms, 1.0e-12f)) + 3.0f;
            const float target = computeGR (lvl, thr, ratio, knee);

            const float c = target > env ? atk : relFast;
            env = c * env + (1.0f - c) * target;
            const float mc = env > mem ? memCharge : memRelease;
            mem = mc * mem + (1.0f - mc) * env;
            const float grDb = std::max (env, mem * 0.6f);

            const float g = dbToGain (-grDb) * gainR.next();
            const float mix = mixR.next();
            float yl = L[i] * g, yr = R[i] * g;
            if (color > 0.0f)
            {
                yl = dc.process (satTanh (yl * tg, bias) * tNorm, 0);
                yr = dc.process (satTanh (yr * tg, bias) * tNorm, 1);
            }
            L[i] = L[i] * (1.0f - mix) + yl * mix;
            R[i] = R[i] * (1.0f - mix) + yr * mix;
            lastGr = grDb;
        }
        meter.store (lastGr);
    }

private:
    float env = 0, mem = 0, rms = 0, lastGr = 0, lastEmph = -1;
    Biquad emph;
    DcBlocker dc;
    Ramp gainR, mixR;
};

//==============================================================================
// 8: Tape
//==============================================================================
class TapeModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void allocate (double maxRate, int) override { delay.allocate ((int) (maxRate * 0.004) + 8); }

    void reset() override
    {
        for (auto& b : f) b.reset();
        dc.reset(); vu.reset(); delay.reset();
        phaseW = phaseF = 0.0;
        inG.init = outG.init = mixR.init = depthR.init = false;
    }

    void onRateChanged() override { dc.setRate (fs); }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        const int speed = idx (p, 2);
        const bool bumpOn = isOn (p, 3);
        if (dirty || speed != lastSpeed || bumpOn != lastBump)
        {
            static const float emphDb[] = { 8.0f, 6.0f, 4.0f };
            static const float bumpHz[] = { 40.0f, 60.0f, 100.0f };
            static const float bumpDb[] = { 2.5f, 2.0f, 1.5f };
            static const float lpHz[]   = { 12000.0f, 18000.0f, 24000.0f };

            f[0].set (design::highShelf (fs, 3000.0, 0.6, emphDb[speed]));
            f[1].set (design::highShelf (fs, 3000.0, 0.6, -emphDb[speed]));
            f[2].set (bumpOn ? design::peak (fs, bumpHz[speed], 1.2, bumpDb[speed]) : design::identity());
            f[3].set (bumpOn ? design::peak (fs, bumpHz[speed] * 2.2, 1.5, -0.8) : design::identity());
            f[4].set (lpHz[speed] < fs * 0.45 ? design::lowpass (fs, lpHz[speed], 0.65) : design::identity());
            lastSpeed = speed; lastBump = bumpOn; dirty = false;
        }

        const float inDb = real (p, 0);
        const float sat = real (p, 1);
        const float k = 0.6f + sat * 0.35f;
        const float bias = 0.04f;
        const float norm = 1.0f / (k * tanhSlope (bias));
        const bool autoGain = isOn (p, 7);
        const float wow = real (p, 4) / 10.0f;

        inG.setTarget (dbToGain (inDb), n);
        outG.setTarget (dbToGain (real (p, 5) - (autoGain ? inDb : 0.0f)), n);
        mixR.setTarget (real (p, 6) * 0.01f, n);
        depthR.setTarget (wow, n);

        const float vuC = timeCoef (300.0f, fs);
        const double wInc = 2.0 * kPi * 0.55 / fs;
        const double fInc = 2.0 * kPi * 6.5 / fs;
        const float msToSmp = (float) (fs * 0.001);

        for (int i = 0; i < n; ++i)
        {
            const float gi = inG.next(), go = outG.next(), mix = mixR.next(), depth = depthR.next();
            const float dryL = L[i], dryR = R[i];
            float y[2];

            for (int c = 0; c < 2; ++c)
            {
                float x = (c == 0 ? dryL : dryR) * gi;
                x = f[0].process (x, c);
                x = satTanh (x * k, bias) * norm;
                x = dc.process (x, c);
                x = f[1].process (x, c);
                x = f[2].process (x, c);
                x = f[3].process (x, c);
                y[c] = f[4].process (x, c);
            }

            if (depth > 0.0001f)
            {
                delay.push (y[0], y[1]);
                const float wowMs = depth * 1.2f, flutMs = depth * 0.04f;
                const float d = (wowMs * (1.0f + (float) std::sin (phaseW))
                               + flutMs * (1.0f + (float) std::sin (phaseF))) * msToSmp;
                y[0] = delay.read (0, d);
                y[1] = delay.read (1, d * 0.98f);
                phaseW += wInc; phaseF += fInc;
                if (phaseW > 2.0 * kPi) phaseW -= 2.0 * kPi;
                if (phaseF > 2.0 * kPi) phaseF -= 2.0 * kPi;
            }
            else
            {
                delay.push (y[0], y[1]);
            }

            L[i] = dryL * (1.0f - mix) + y[0] * go * mix;
            R[i] = dryR * (1.0f - mix) + y[1] * go * mix;
            vu.add (L[i], R[i], vuC);
        }
        meter.store (vu.db());
    }

private:
    Biquad f[5];
    DcBlocker dc;
    VuFollower vu;
    DelayLine delay;
    double phaseW = 0, phaseF = 0;
    int lastSpeed = -1;
    bool lastBump = false;
    Ramp inG, outG, mixR, depthR;
};

//==============================================================================
// 9: Gate
//==============================================================================
class GateModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { hp.reset(); lp.reset(); env = 0; gDb = -80; open = false; hold = 0; }

    void process (float* L, float* R, const float* eL, const float* eR, int n, const float* p) override
    {
        const float hpF = real (p, 5), lpF = real (p, 6);
        const bool hpOn = ! info.params[5].isOff (hpF);
        const bool lpOn = ! info.params[6].isOff (lpF) && lpF < fs * 0.45;
        if (dirty || hpF != lastHp || lpF != lastLp)
        {
            hp.set (design::highpass (fs, hpF));
            lp.set (lpOn ? design::lowpass (fs, lpF) : design::identity());
            lastHp = hpF; lastLp = lpF; dirty = false;
        }

        const float thr = real (p, 0);
        const float range = real (p, 1);
        const float atk = timeCoef (real (p, 2), fs);
        const int holdSmp = (int) (real (p, 3) * 0.001f * fs);
        const float rel = timeCoef (real (p, 4), fs);
        const bool ext = isOn (p, 7) && eL != nullptr;
        const bool expander = idx (p, 8) == 1;
        const float detRel = timeCoef (8.0f, fs);

        for (int i = 0; i < n; ++i)
        {
            float kl = ext ? eL[i] : L[i];
            float kr = ext ? eR[i] : R[i];
            if (hpOn) { kl = hp.process (kl, 0); kr = hp.process (kr, 1); }
            if (lpOn) { kl = lp.process (kl, 0); kr = lp.process (kr, 1); }

            const float a = std::max (std::abs (kl), std::abs (kr));
            env = a > env ? a : detRel * env + (1.0f - detRel) * a;
            const float lvl = gainToDb (env);

            float target;
            if (expander)
            {
                target = lvl < thr ? std::max (-range, (lvl - thr) * 1.5f) : 0.0f;
            }
            else
            {
                if (lvl > thr) { open = true; hold = holdSmp; }
                else if (lvl < thr - 4.0f)
                {
                    if (hold > 0) --hold;
                    else open = false;
                }
                target = open ? 0.0f : -range;
            }

            const float c = target > gDb ? atk : rel;
            gDb = c * gDb + (1.0f - c) * target;
            const float g = dbToGain (gDb);
            L[i] *= g; R[i] *= g;
        }
        meter.store (-gDb);
    }

private:
    Biquad hp, lp;
    float env = 0, gDb = -80, lastHp = -1, lastLp = -1;
    bool open = false;
    int hold = 0;
};

//==============================================================================
// 10: De-Esser
//==============================================================================
class DeEsserModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { split.reset(); env = 0; gr.reset(); }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        const float freq = real (p, 0);
        if (dirty || freq != lastF)
        {
            split.set (design::highpass (fs, freq, 0.7071));
            lastF = freq; dirty = false;
        }

        const float thr = real (p, 1);
        const float range = real (p, 2);
        const bool wide = idx (p, 3) == 1;
        const bool listen = isOn (p, 4);
        const float atk = timeCoef (0.5f, fs);
        const float rel = timeCoef (60.0f, fs);
        const float detRel = timeCoef (20.0f, fs);

        for (int i = 0; i < n; ++i)
        {
            const float hl = split.process (L[i], 0);
            const float hr = split.process (R[i], 1);
            const float a = std::max (std::abs (hl), std::abs (hr));
            env = a > env ? a : detRel * env + (1.0f - detRel) * a;

            const float target = std::min (range, computeGR (gainToDb (env), thr, 4.0f, 3.0f));
            const float e = gr.process (target, atk, rel);
            const float g = dbToGain (-e);

            if (listen)        { L[i] = hl; R[i] = hr; }
            else if (wide)     { L[i] *= g; R[i] *= g; }
            else
            {
                L[i] = (L[i] - hl) + hl * g;
                R[i] = (R[i] - hr) + hr * g;
            }
        }
        meter.store (gr.env);
    }

private:
    Biquad split;
    GrEnvelope gr;
    float env = 0, lastF = -1;
};

//==============================================================================
// 11: Limiter
//==============================================================================
class LimiterModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { gL = gR = 1.0f; inG.init = false; }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        const float ceiling = dbToGain (real (p, 1));
        const float rel = timeCoef (real (p, 2), fs);
        const int character = idx (p, 3);
        const float link = real (p, 4) * 0.01f;
        inG.setTarget (dbToGain (real (p, 0)), n);

        const float knee = character == 0 ? 1.0f : 0.85f;  // soft characters limit slightly below and saturate into the ceiling
        const float limitAt = ceiling * knee;

        for (int i = 0; i < n; ++i)
        {
            const float gi = inG.next();
            float xl = L[i] * gi, xr = R[i] * gi;

            if (character == 2) // Loud: pre-clip transients
            {
                const float c = ceiling * 1.4f;
                xl = c * std::tanh (xl / c);
                xr = c * std::tanh (xr / c);
            }

            const float al = std::abs (xl), ar = std::abs (xr);
            const float m = std::max (al, ar);
            const float pl = lerp (al, m, link), pr = lerp (ar, m, link);
            const float tl = pl > limitAt ? limitAt / pl : 1.0f;
            const float tr = pr > limitAt ? limitAt / pr : 1.0f;

            gL = tl < gL ? tl : rel * gL + (1.0f - rel) * tl;
            gR = tr < gR ? tr : rel * gR + (1.0f - rel) * tr;

            float yl = xl * gL, yr = xr * gR;

            if (character == 0)
            {
                yl = clampf (yl, -ceiling, ceiling);
                yr = clampf (yr, -ceiling, ceiling);
            }
            else
            {
                yl = softCeil (yl, limitAt, ceiling);
                yr = softCeil (yr, limitAt, ceiling);
            }
            L[i] = yl; R[i] = yr;
        }
        meter.store (-gainToDb (std::min (gL, gR)));
    }

private:
    static float softCeil (float x, float kneeStart, float ceiling)
    {
        const float a = std::abs (x);
        if (a <= kneeStart) return x;
        const float span = ceiling - kneeStart;
        const float y = kneeStart + span * std::tanh ((a - kneeStart) / span);
        return x < 0 ? -y : y;
    }

    float gL = 1.0f, gR = 1.0f;
    Ramp inG;
};

//==============================================================================
// 12: Transient shaper
//==============================================================================
class TransientModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { e1 = e2 = e3 = 0; gSm = 0; outG.init = mixR.init = false; peak.v = 0; }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        static const float speeds[] = { 0.5f, 1.0f, 2.0f };
        const float s = speeds[idx (p, 2)];
        const float att = real (p, 0) / 100.0f;
        const float sus = real (p, 1) / 100.0f;

        const float a1 = timeCoef (0.3f * s, fs),  r1 = timeCoef (80.0f * s, fs);
        const float a2 = timeCoef (20.0f * s, fs), r2 = timeCoef (80.0f * s, fs);
        const float a3 = timeCoef (0.3f * s, fs),  r3 = timeCoef (400.0f * s, fs);
        const float sm = timeCoef (1.0f, fs);

        outG.setTarget (dbToGain (real (p, 3)), n);
        mixR.setTarget (real (p, 4) * 0.01f, n);

        for (int i = 0; i < n; ++i)
        {
            const float a = std::max (std::abs (L[i]), std::abs (R[i])) + 1.0e-6f;
            e1 = (a > e1 ? a1 : r1) * (e1 - a) + a;
            e2 = (a > e2 ? a2 : r2) * (e2 - a) + a;
            e3 = (a > e3 ? a3 : r3) * (e3 - a) + a;

            const float trans = std::max (0.0f, gainToDb (e1) - gainToDb (e2));
            const float tail  = std::max (0.0f, gainToDb (e3) - gainToDb (e1));
            const float target = clampf (att * trans * 1.5f + sus * tail, -24.0f, 18.0f);
            gSm = sm * gSm + (1.0f - sm) * target;

            const float g = dbToGain (gSm) * outG.next();
            const float mix = mixR.next();
            L[i] = L[i] * (1.0f - mix) + L[i] * g * mix;
            R[i] = R[i] * (1.0f - mix) + R[i] * g * mix;
            peak.add (L[i], R[i]);
        }
        peak.publish (meter, fs, n);
    }

private:
    float e1 = 0, e2 = 0, e3 = 0, gSm = 0;
    PeakHold peak;
    Ramp outG, mixR;
};

//==============================================================================
// 13: Air Lift
//==============================================================================
class AirLiftModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { for (auto& b : f) b.reset(); outG.init = false; }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        bool changed = dirty;
        for (int i = 0; i < 5; ++i)
            if (p[i] != last[i]) { changed = true; last[i] = p[i]; }

        if (changed)
        {
            const float airF = real (p, 0);
            f[0].set (design::highShelf (fs, airF, 0.5, real (p, 1)));
            f[1].set (design::lowShelf (fs, real (p, 3), 0.6, real (p, 4)));
            f[2].set (design::highpass (fs, airF * 0.5, 0.7071));
            f[3].set (design::highpass (fs, airF * 0.5, 0.7071));
            dirty = false;
        }

        const float harm = real (p, 2) / 10.0f;
        outG.setTarget (dbToGain (real (p, 5)), n);

        for (int i = 0; i < n; ++i)
        {
            const float go = outG.next();
            float* io[2] = { L + i, R + i };
            for (int c = 0; c < 2; ++c)
            {
                float x = *io[c];
                if (harm > 0.0f)
                {
                    const float h = f[2].process (x, c) * 2.0f;
                    const float gen = (std::tanh (h * 2.0f) * 0.5f - h) + 0.6f * h * std::abs (h);
                    x += f[3].process (gen, c) * harm * 0.6f;
                }
                x = f[0].process (x, c);
                x = f[1].process (x, c);
                *io[c] = x * go;
            }
        }
    }

private:
    Biquad f[4];
    float last[5] { -1, -1, -1, -1, -1 };
    Ramp outG;
};

//==============================================================================
// 14: Trimmer
//==============================================================================
class TrimmerModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { bass.reset(); gR.init = panL.init = panR.init = widthR.init = false; peak.v = 0; }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        const float bassF = real (p, 3);
        const bool bassOn = ! info.params[3].isOff (bassF);
        if (dirty || bassF != lastBass)
        {
            bass.set (design::lowpass (fs, bassF, 0.7071));
            lastBass = bassF; dirty = false;
        }

        const float pan = real (p, 1) / 100.0f;
        gR.setTarget (dbToGain (real (p, 0)), n);
        panL.setTarget (pan > 0.0f ? 1.0f - pan : 1.0f, n);
        panR.setTarget (pan < 0.0f ? 1.0f + pan : 1.0f, n);
        widthR.setTarget (real (p, 2) * 0.01f, n);

        const float invL = isOn (p, 4) ? -1.0f : 1.0f;
        const float invR = isOn (p, 5) ? -1.0f : 1.0f;
        const bool mono = isOn (p, 6);
        const bool swap = isOn (p, 7);

        for (int i = 0; i < n; ++i)
        {
            const float g = gR.next(), pl = panL.next(), pr = panR.next(), w = widthR.next();
            float l = L[i] * invL, r = R[i] * invR;
            if (swap) std::swap (l, r);

            const float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r) * (mono ? 0.0f : w);
            if (bassOn)
            {
                const float low = bass.process (side, 0);
                side -= low;
            }
            l = (mid + side) * g * pl;
            r = (mid - side) * g * pr;
            L[i] = l; R[i] = r;
            peak.add (l, r);
        }
        peak.publish (meter, fs, n);
    }

private:
    Biquad bass;
    float lastBass = -1;
    PeakHold peak;
    Ramp gR, panL, panR, widthR;
};

//==============================================================================
// 15: Harmonics
//==============================================================================
class HarmonicsModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;

    void reset() override { for (auto& b : tone) b.reset(); dc.reset(); outG.init = mixR.init = false; peak.v = 0; }
    void onRateChanged() override { dc.setRate (fs); }

    void process (float* L, float* R, const float*, const float*, int n, const float* p) override
    {
        const float t = real (p, 2) / 100.0f;
        if (dirty || t != lastTone)
        {
            tone[0].set (design::lowShelf (fs, 800.0, 0.5, -t * 6.0));
            tone[1].set (design::highShelf (fs, 800.0, 0.5, t * 6.0));
            lastTone = t; dirty = false;
        }

        const float g = dbToGain (real (p, 0));
        const int type = idx (p, 1);
        const bool autoGain = isOn (p, 5);
        const float post = autoGain ? 1.0f / g : 1.0f;
        outG.setTarget (dbToGain (real (p, 3)), n);
        mixR.setTarget (real (p, 4) * 0.01f, n);
        const float tubeBias = 0.2f, tubeNorm = 1.0f / tanhSlope (tubeBias);

        for (int i = 0; i < n; ++i)
        {
            const float go = outG.next(), mix = mixR.next();
            float* io[2] = { L + i, R + i };
            for (int c = 0; c < 2; ++c)
            {
                const float dry = *io[c];
                const float u = dry * g;
                float y;
                switch (type)
                {
                    case 0:  y = satTanh (u, tubeBias) * tubeNorm; break;
                    case 1:  y = std::atan (u); break;
                    case 2:  y = u > 0 ? u / (1.0f + u) : u / (1.0f - 0.8f * u); break;
                    case 3:  y = clampf (u, -1.0f, 1.0f); break;
                    default: y = std::sin (u); break;
                }
                y = dc.process (y * post, c);
                if (t != 0.0f) y = tone[1].process (tone[0].process (y, c), c);
                *io[c] = dry * (1.0f - mix) + y * go * mix;
            }
            peak.add (L[i], R[i]);
        }
        peak.publish (meter, fs, n);
    }

private:
    Biquad tone[2];
    DcBlocker dc;
    PeakHold peak;
    float lastTone = 1.0e9f;
    Ramp outG, mixR;
};

//==============================================================================
class EmptyModule final : public ModuleDSP
{
public:
    using ModuleDSP::ModuleDSP;
    void process (float*, float*, const float*, const float*, int, const float*) override {}
};

} // namespace

std::unique_ptr<ModuleDSP> createModuleDSP (int type)
{
    const auto& info = getModules()[(size_t) juce::jlimit (0, numModuleTypes() - 1, type)];
    switch (type)
    {
        case 1:  return std::make_unique<ChannelModule> (info);
        case 2:  return std::make_unique<QuadEqModule> (info);
        case 3:  return std::make_unique<HeritageEqModule> (info);
        case 4:  return std::make_unique<ProgramEqModule> (info);
        case 5:  return std::make_unique<BusCompModule> (info);
        case 6:  return std::make_unique<StressCompModule> (info);
        case 7:  return std::make_unique<OptoCompModule> (info);
        case 8:  return std::make_unique<TapeModule> (info);
        case 9:  return std::make_unique<GateModule> (info);
        case 10: return std::make_unique<DeEsserModule> (info);
        case 11: return std::make_unique<LimiterModule> (info);
        case 12: return std::make_unique<TransientModule> (info);
        case 13: return std::make_unique<AirLiftModule> (info);
        case 14: return std::make_unique<TrimmerModule> (info);
        case 15: return std::make_unique<HarmonicsModule> (info);
        default: return std::make_unique<EmptyModule> (info);
    }
}

} // namespace mr
