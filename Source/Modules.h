#pragma once

#include "ModuleDefs.h"
#include "Dsp.h"
#include <atomic>
#include <memory>

namespace mr
{
/** Base class for every rack module's audio processing.
    Modules are pre-allocated for every slot so switching never allocates on the audio thread. */
class ModuleDSP
{
public:
    explicit ModuleDSP (const ModuleInfo& i) : info (i) {}
    virtual ~ModuleDSP() = default;

    /** Allocate memory. maxRate is the highest sample rate the module can be run at. */
    virtual void allocate (double /*maxRate*/, int /*maxBlock*/) {}

    /** Change the running sample rate. Must not allocate. */
    void setRate (double sr) { fs = sr; dirty = true; onRateChanged(); }

    virtual void reset() {}

    /** Process in place. 'p' holds kParamsPerSlot normalised values.
        scL/scR is the external side-chain (nullptr when unavailable). */
    virtual void process (float* L, float* R, const float* scL, const float* scR, int n, const float* p) = 0;

    std::atomic<float> meter { 0.0f };

protected:
    virtual void onRateChanged() {}
    float real (const float* p, int i) const { return info.params[(size_t) i].toReal (p[i]); }
    bool  isOn (const float* p, int i) const { return p[i] >= 0.5f; }
    int   idx (const float* p, int i) const  { return (int) real (p, i); }

    const ModuleInfo& info;
    double fs = 44100.0;
    bool dirty = true;
};

std::unique_ptr<ModuleDSP> createModuleDSP (int type);

} // namespace mr
