#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mr;

namespace
{
    constexpr int kDefaultTypes[kNumSlots] = { 1, 2, 5, 0, 0, 0, 0, 0 };

    juce::String dbText (float v)
    {
        return (v > 0.049f ? "+" : "") + juce::String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB";
    }
}

//==============================================================================
MultiRackProcessor::MultiRackProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      presets (*this)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        typeParams[(size_t) s] = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (slotParamId (s, "type")));
        onParams[(size_t) s]   = apvts.getParameter (slotParamId (s, "on"));
        soloParams[(size_t) s] = apvts.getParameter (slotParamId (s, "solo"));
        outParams[(size_t) s]  = apvts.getParameter (slotParamId (s, "out"));
        onRaw[(size_t) s]      = apvts.getRawParameterValue (slotParamId (s, "on"));
        soloRaw[(size_t) s]    = apvts.getRawParameterValue (slotParamId (s, "solo"));
        outRaw[(size_t) s]     = apvts.getRawParameterValue (slotParamId (s, "out"));

        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            knobParams[(size_t) s][(size_t) i] = apvts.getParameter (slotKnobId (s, i));
            knobRaw[(size_t) s][(size_t) i]    = apvts.getRawParameterValue (slotKnobId (s, i));
        }

        slotTypeCache[(size_t) s].store (typeParams[(size_t) s]->getIndex());
        pendingType[(size_t) s].store (-1);
        slotMeters[(size_t) s].store (0.0f);
        apvts.addParameterListener (slotParamId (s, "type"), this);

        for (int t = 0; t < numModuleTypes(); ++t)
            slots[(size_t) s].modules.push_back (createModuleDSP (t));
    }

    for (int m = 0; m < kNumMacros; ++m)
    {
        macroParams[(size_t) m] = apvts.getRawParameterValue (macroId (m));
        macroNames[(size_t) m] = "Macro " + juce::String (m + 1);
    }

    osMain = std::make_unique<juce::dsp::Oversampling<float>> (2, kOversampleLog2,
                                                               juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    osSide = std::make_unique<juce::dsp::Oversampling<float>> (2, kOversampleLog2,
                                                               juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    latency = juce::roundToInt (osMain->getLatencyInSamples());
    setLatencySamples (latency);

    inGainRaw  = apvts.getRawParameterValue ("input");
    outGainRaw = apvts.getRawParameterValue ("output");
    mixRaw     = apvts.getRawParameterValue ("mix");
    hqRaw      = apvts.getRawParameterValue ("hq");
}

MultiRackProcessor::~MultiRackProcessor()
{
    for (int s = 0; s < kNumSlots; ++s)
        apvts.removeParameterListener (slotParamId (s, "type"), this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MultiRackProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto gainAttr = juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return dbText (v); })
                        .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.-+").getFloatValue(); });

    auto global = std::make_unique<juce::AudioProcessorParameterGroup> ("global", "Global", "|");
    global->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "input", 1 }, "Input Gain",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, gainAttr));
    global->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output Gain",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, gainAttr));
    global->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Rack Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                                   juce::AudioParameterFloatAttributes()
                                                                       .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + " %"; })));
    global->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "hq", 1 }, "HQ Oversampling", false));

    for (int m = 0; m < kNumMacros; ++m)
        global->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { macroId (m), 1 },
                                                                       "Macro " + juce::String (m + 1),
                                                                       juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                                       juce::AudioParameterFloatAttributes()
                                                                           .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; })));
    layout.add (std::move (global));

    const auto& modules = getModules();

    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto letter = slotLetter (s);
        const int defType = kDefaultTypes[s];
        const auto& defInfo = modules[(size_t) defType];
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("slot" + letter, "Slot " + letter, "|");

        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotParamId (s, "type"), 1 },
                                                                       letter + " Module", getModuleNames(), defType));
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotParamId (s, "on"), 1 },
                                                                     letter + " Power", true));
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotParamId (s, "solo"), 1 },
                                                                     letter + " Solo", false));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotParamId (s, "out"), 1 },
                                                                      letter + " Out Gain",
                                                                      juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f), 0.0f, gainAttr));

        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            const float def = i < (int) defInfo.params.size() ? defInfo.params[(size_t) i].defaultNorm() : 0.0f;

            auto attr = juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([this, s, i] (float v, int)
                {
                    const auto& info = getModules()[(size_t) slotTypeCache[(size_t) s].load()];
                    if (i < (int) info.params.size())
                        return info.params[(size_t) i].toText (v);
                    return juce::String ("-");
                })
                .withValueFromStringFunction ([this, s, i] (const juce::String& text)
                {
                    const auto& info = getModules()[(size_t) slotTypeCache[(size_t) s].load()];
                    if (i < (int) info.params.size())
                        return info.params[(size_t) i].fromText (text);
                    return text.getFloatValue();
                });

            group->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotKnobId (s, i), 1 },
                                                                          letter + " Param " + juce::String (i + 1),
                                                                          juce::NormalisableRange<float> (0.0f, 1.0f), def, attr));
        }
        layout.add (std::move (group));
    }

    return layout;
}

void MultiRackProcessor::parameterChanged (const juce::String& id, float newValue)
{
    for (int s = 0; s < kNumSlots; ++s)
        if (id == slotParamId (s, "type"))
            slotTypeCache[(size_t) s].store (juce::jlimit (0, numModuleTypes() - 1, juce::roundToInt (newValue)));
}

//==============================================================================
bool MultiRackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void MultiRackProcessor::prepareToPlay (double sampleRate, int)
{
    baseRate = sampleRate;

    work.setSize (2, kSubBlock);
    scWork.setSize (2, kSubBlock);
    dryWork.setSize (2, kSubBlock);

    osMain->initProcessing ((size_t) kSubBlock);
    osSide->initProcessing ((size_t) kSubBlock);

    // Constant latency (the HQ oversampling latency) whether HQ is on or off, so toggling HQ never
    // has to ask the host to re-negotiate latency from the audio thread.
    latency = juce::roundToInt (osMain->getLatencyInSamples());
    setLatencySamples (latency);
    wetDelay.allocate (1024);

    dryDelay.allocate (1024);

    const double maxRate = sampleRate * (1 << kOversampleLog2);
    for (auto& slot : slots)
    {
        for (auto& m : slot.modules)
            m->allocate (maxRate, kSubBlock << kOversampleLog2);
        slot.activeType = -1;
        slot.outRamp.init = false;
    }

    inRamp.init = outRamp.init = mixRamp.init = false;
    setRunRate (hqRaw->load() >= 0.5f);
    prepared = true;
}

void MultiRackProcessor::setRunRate (bool hq)
{
    hqActive = hq;
    const double rate = hq ? baseRate * (1 << kOversampleLog2) : baseRate;

    for (auto& slot : slots)
        for (auto& m : slot.modules)
        {
            m->setRate (rate);
            m->reset();
        }

    if (osMain != nullptr) osMain->reset();
    if (osSide != nullptr) osSide->reset();
    dryDelay.reset();
    wetDelay.reset();
}

//==============================================================================
void MultiRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const bool hq = hqRaw->load() >= 0.5f;
    if (hq != hqActive)
        setRunRate (hq);

    const bool hasSidechain = getBusCount (true) > 1
                              && getBus (true, 1) != nullptr
                              && getBus (true, 1)->isEnabled()
                              && getBus (true, 1)->getNumberOfChannels() > 0;

    // Clear any output channels that have no input.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += kSubBlock)
        processSubBlock (buffer, start, juce::jmin (kSubBlock, total - start), hasSidechain);
}

void MultiRackProcessor::processSubBlock (juce::AudioBuffer<float>& buffer, int start, int num, bool hasSidechain)
{
    auto mainIn  = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int inCh = mainIn.getNumChannels();
    const int outCh = mainOut.getNumChannels();

    float* wl = work.getWritePointer (0);
    float* wr = work.getWritePointer (1);

    if (inCh >= 2)
    {
        juce::FloatVectorOperations::copy (wl, mainIn.getReadPointer (0, start), num);
        juce::FloatVectorOperations::copy (wr, mainIn.getReadPointer (1, start), num);
    }
    else if (inCh == 1)
    {
        juce::FloatVectorOperations::copy (wl, mainIn.getReadPointer (0, start), num);
        juce::FloatVectorOperations::copy (wr, mainIn.getReadPointer (0, start), num);
    }
    else
    {
        juce::FloatVectorOperations::clear (wl, num);
        juce::FloatVectorOperations::clear (wr, num);
    }

    const float* scL = nullptr;
    const float* scR = nullptr;
    if (hasSidechain)
    {
        auto sc = getBusBuffer (buffer, true, 1);
        const int scCh = sc.getNumChannels();
        juce::FloatVectorOperations::copy (scWork.getWritePointer (0), sc.getReadPointer (0, start), num);
        juce::FloatVectorOperations::copy (scWork.getWritePointer (1), sc.getReadPointer (scCh > 1 ? 1 : 0, start), num);
    }

    // Input gain + input meter
    inRamp.setTarget (dsp::dbToGain (inGainRaw->load()), num);
    float inPeak = 0.0f;
    for (int i = 0; i < num; ++i)
    {
        const float g = inRamp.next();
        wl[i] *= g; wr[i] *= g;
        inPeak = std::max (inPeak, std::max (std::abs (wl[i]), std::abs (wr[i])));
    }
    inMeter.store (std::max (dsp::gainToDb (inPeak), inMeter.load() - 0.8f * (float) num / 256.0f));

    // Dry path (latency aligned)
    float* dl = dryWork.getWritePointer (0);
    float* dr = dryWork.getWritePointer (1);
    for (int i = 0; i < num; ++i)
    {
        dryDelay.push (wl[i], wr[i]);
        dl[i] = dryDelay.read (0, (float) latency);
        dr[i] = dryDelay.read (1, (float) latency);
    }

    // Rack
    if (hqActive)
    {
        juce::dsp::AudioBlock<float> block (work);
        auto sub = block.getSubBlock (0, (size_t) num);
        auto up = osMain->processSamplesUp (sub);

        const float* uscL = nullptr;
        const float* uscR = nullptr;
        if (hasSidechain)
        {
            juce::dsp::AudioBlock<float> scBlock (scWork);
            auto scUp = osSide->processSamplesUp (scBlock.getSubBlock (0, (size_t) num));
            uscL = scUp.getChannelPointer (0);
            uscR = scUp.getChannelPointer (1);
        }

        runRack (up.getChannelPointer (0), up.getChannelPointer (1), uscL, uscR, (int) up.getNumSamples());
        osMain->processSamplesDown (sub);
    }
    else
    {
        if (hasSidechain)
        {
            scL = scWork.getReadPointer (0);
            scR = scWork.getReadPointer (1);
        }
        runRack (wl, wr, scL, scR, num);

        // keep the same latency as HQ mode
        for (int i = 0; i < num; ++i)
        {
            wetDelay.push (wl[i], wr[i]);
            wl[i] = wetDelay.read (0, (float) latency);
            wr[i] = wetDelay.read (1, (float) latency);
        }
    }

    // Output gain, global mix, output meter
    outRamp.setTarget (dsp::dbToGain (outGainRaw->load()), num);
    mixRamp.setTarget (mixRaw->load() * 0.01f, num);
    float outPeak = 0.0f;
    for (int i = 0; i < num; ++i)
    {
        const float g = outRamp.next(), mix = mixRamp.next();
        wl[i] = (dl[i] * (1.0f - mix) + wl[i] * mix) * g;
        wr[i] = (dr[i] * (1.0f - mix) + wr[i] * mix) * g;
        outPeak = std::max (outPeak, std::max (std::abs (wl[i]), std::abs (wr[i])));
    }
    outMeter.store (std::max (dsp::gainToDb (outPeak), outMeter.load() - 0.8f * (float) num / 256.0f));

    if (outCh >= 2)
    {
        juce::FloatVectorOperations::copy (mainOut.getWritePointer (0, start), wl, num);
        juce::FloatVectorOperations::copy (mainOut.getWritePointer (1, start), wr, num);
    }
    else if (outCh == 1)
    {
        float* o = mainOut.getWritePointer (0, start);
        for (int i = 0; i < num; ++i)
            o[i] = 0.5f * (wl[i] + wr[i]);
    }
}

void MultiRackProcessor::runRack (float* L, float* R, const float* scL, const float* scR, int n)
{
    bool anySolo = false;
    for (int s = 0; s < kNumSlots; ++s)
        if (slotTypeCache[(size_t) s].load() != 0 && onRaw[(size_t) s]->load() >= 0.5f && soloRaw[(size_t) s]->load() >= 0.5f)
            anySolo = true;

    float macroVals[kNumMacros];
    for (int m = 0; m < kNumMacros; ++m)
        macroVals[m] = macroParams[(size_t) m]->load();

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& slot = slots[(size_t) s];
        const int type = juce::jlimit (0, numModuleTypes() - 1, typeParams[(size_t) s]->getIndex());

        // A module swap from the UI is in progress: pass the slot through until the type lands.
        const int pending = pendingType[(size_t) s].load();
        if (pending >= 0)
        {
            if (pending != type) continue;
            int expected = pending;
            pendingType[(size_t) s].compare_exchange_strong (expected, -1);
        }

        auto& module = *slot.modules[(size_t) type];
        if (type != slot.activeType)
        {
            module.reset();
            slot.activeType = type;
            slot.outRamp.snap (dsp::dbToGain (outRaw[(size_t) s]->load()));
        }

        const bool active = type != 0
                            && onRaw[(size_t) s]->load() >= 0.5f
                            && (! anySolo || soloRaw[(size_t) s]->load() >= 0.5f);
        if (! active)
        {
            slotMeters[(size_t) s].store (module.meter.load());
            continue;
        }

        float p[kParamsPerSlot];
        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            const auto& link = links[(size_t) s][(size_t) i];
            const int m = link.macro.load (std::memory_order_relaxed);
            if (m >= 0 && m < kNumMacros)
                p[i] = juce::jlimit (0.0f, 1.0f, link.lo.load() + (link.hi.load() - link.lo.load()) * macroVals[m]);
            else
                p[i] = knobRaw[(size_t) s][(size_t) i]->load (std::memory_order_relaxed);
        }

        module.process (L, R, scL, scR, n, p);

        slot.outRamp.setTarget (dsp::dbToGain (outRaw[(size_t) s]->load()), n);
        if (slot.outRamp.left > 0 || slot.outRamp.v != 1.0f)
            for (int i = 0; i < n; ++i)
            {
                const float g = slot.outRamp.next();
                L[i] *= g; R[i] *= g;
            }

        slotMeters[(size_t) s].store (module.meter.load());
    }
}

void MultiRackProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int n = buffer.getNumSamples();
    const int chs = juce::jmin (2, mainOut.getNumChannels());

    for (int i = 0; i < n; ++i)
    {
        const float l = mainIn.getNumChannels() > 0 ? mainIn.getSample (0, i) : 0.0f;
        const float r = mainIn.getNumChannels() > 1 ? mainIn.getSample (1, i) : l;
        dryDelay.push (l, r);
        if (chs > 0) mainOut.setSample (0, i, dryDelay.read (0, (float) latency));
        if (chs > 1) mainOut.setSample (1, i, dryDelay.read (1, (float) latency));
    }
}

//==============================================================================
void MultiRackProcessor::setParamNotifying (juce::RangedAudioParameter* p, float norm)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (norm);
    p->endChangeGesture();
}

void MultiRackProcessor::setSlotModule (int slot, int type)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots)) return;
    type = juce::jlimit (0, numModuleTypes() - 1, type);
    const auto& info = getModules()[(size_t) type];

    pendingType[(size_t) slot].store (type);
    slotTypeCache[(size_t) slot].store (type);

    for (int i = 0; i < kParamsPerSlot; ++i)
    {
        links[(size_t) slot][(size_t) i].macro.store (-1);
        const float def = i < (int) info.params.size() ? info.params[(size_t) i].defaultNorm() : 0.0f;
        setParamNotifying (knobParams[(size_t) slot][(size_t) i], def);
    }
    setParamNotifying (onParams[(size_t) slot], 1.0f);
    setParamNotifying (soloParams[(size_t) slot], 0.0f);
    setParamNotifying (outParams[(size_t) slot], outParams[(size_t) slot]->convertTo0to1 (0.0f));
    setParamNotifying (typeParams[(size_t) slot], typeParams[(size_t) slot]->convertTo0to1 ((float) type));
}

void MultiRackProcessor::resetSlot (int slot)
{
    setSlotModule (slot, getSlotType (slot));
}

void MultiRackProcessor::swapSlots (int a, int b)
{
    if (a == b || ! juce::isPositiveAndBelow (a, kNumSlots) || ! juce::isPositiveAndBelow (b, kNumSlots))
        return;

    struct Snapshot
    {
        float type, on, solo, out;
        float knobs[kParamsPerSlot];
        int macro[kParamsPerSlot];
        float lo[kParamsPerSlot], hi[kParamsPerSlot];
    };

    auto take = [this] (int s)
    {
        Snapshot sn {};
        sn.type = static_cast<juce::AudioProcessorParameter*> (typeParams[(size_t) s])->getValue();
        sn.on   = onParams[(size_t) s]->getValue();
        sn.solo = soloParams[(size_t) s]->getValue();
        sn.out  = outParams[(size_t) s]->getValue();
        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            sn.knobs[i] = knobParams[(size_t) s][(size_t) i]->getValue();
            auto& l = links[(size_t) s][(size_t) i];
            sn.macro[i] = l.macro.load(); sn.lo[i] = l.lo.load(); sn.hi[i] = l.hi.load();
        }
        return sn;
    };

    auto put = [this] (int s, const Snapshot& sn)
    {
        const int type = juce::roundToInt (typeParams[(size_t) s]->convertFrom0to1 (sn.type));
        pendingType[(size_t) s].store (type);
        slotTypeCache[(size_t) s].store (type);
        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            setParamNotifying (knobParams[(size_t) s][(size_t) i], sn.knobs[i]);
            auto& l = links[(size_t) s][(size_t) i];
            l.lo.store (sn.lo[i]); l.hi.store (sn.hi[i]); l.macro.store (sn.macro[i]);
        }
        setParamNotifying (onParams[(size_t) s], sn.on);
        setParamNotifying (soloParams[(size_t) s], sn.solo);
        setParamNotifying (outParams[(size_t) s], sn.out);
        setParamNotifying (typeParams[(size_t) s], sn.type);
        // If the type did not actually change the param listener will not fire: clear pending ourselves.
        if (typeParams[(size_t) s]->getIndex() == type)
            pendingType[(size_t) s].store (-1);
    };

    const auto sa = take (a), sb = take (b);
    put (a, sb);
    put (b, sa);
}

void MultiRackProcessor::moveSlot (int from, int to)
{
    if (from == to) return;
    if (from < to) for (int s = from; s < to; ++s) swapSlots (s, s + 1);
    else           for (int s = from; s > to; --s) swapSlots (s, s - 1);
}

void MultiRackProcessor::clearRack()
{
    for (int s = 0; s < kNumSlots; ++s)
        setSlotModule (s, 0);
}

void MultiRackProcessor::copySlotToClipboard (int slot)
{
    juce::ValueTree t ("SLOT");
    t.setProperty ("type", getModules()[(size_t) getSlotType (slot)].id, nullptr);
    for (int i = 0; i < kParamsPerSlot; ++i)
        t.setProperty ("p" + juce::String (i), knobParams[(size_t) slot][(size_t) i]->getValue(), nullptr);
    t.setProperty ("out", outParams[(size_t) slot]->getValue(), nullptr);
    slotClipboard = t;
}

void MultiRackProcessor::pasteSlotFromClipboard (int slot)
{
    if (! slotClipboard.isValid()) return;
    const int type = findModuleById (slotClipboard["type"].toString());
    setSlotModule (slot, type);
    for (int i = 0; i < kParamsPerSlot; ++i)
        setParamNotifying (knobParams[(size_t) slot][(size_t) i], (float) slotClipboard["p" + juce::String (i)]);
    setParamNotifying (outParams[(size_t) slot], (float) slotClipboard["out"]);
}

//==============================================================================
void MultiRackProcessor::assignMacro (int slot, int index, int macro)
{
    auto& l = links[(size_t) slot][(size_t) index];
    l.lo.store (0.0f);
    l.hi.store (1.0f);
    l.macro.store (macro);
}

void MultiRackProcessor::setMacroRange (int slot, int index, float lo, float hi)
{
    auto& l = links[(size_t) slot][(size_t) index];
    l.lo.store (juce::jlimit (0.0f, 1.0f, lo));
    l.hi.store (juce::jlimit (0.0f, 1.0f, hi));
}

void MultiRackProcessor::removeMacroLink (int slot, int index)
{
    auto& l = links[(size_t) slot][(size_t) index];
    const int m = l.macro.load();
    if (m >= 0)
    {
        // Leave the knob where the macro had it.
        const float eff = l.lo.load() + (l.hi.load() - l.lo.load()) * getMacroValue (m);
        l.macro.store (-1);
        setParamNotifying (knobParams[(size_t) slot][(size_t) index], eff);
    }
}

void MultiRackProcessor::clearMacro (int macro)
{
    for (int s = 0; s < kNumSlots; ++s)
        for (int i = 0; i < kParamsPerSlot; ++i)
            if (links[(size_t) s][(size_t) i].macro.load() == macro)
                removeMacroLink (s, i);
}

//==============================================================================
juce::ValueTree MultiRackProcessor::getFullState()
{
    auto state = apvts.copyState();

    auto old = state.getChildWithName ("MACROS");
    if (old.isValid()) state.removeChild (old, nullptr);

    juce::ValueTree macros ("MACROS");
    for (int m = 0; m < kNumMacros; ++m)
        macros.setProperty ("name" + juce::String (m), macroNames[(size_t) m], nullptr);

    for (int s = 0; s < kNumSlots; ++s)
        for (int i = 0; i < kParamsPerSlot; ++i)
        {
            const auto& l = links[(size_t) s][(size_t) i];
            if (l.macro.load() >= 0)
            {
                juce::ValueTree link ("LINK");
                link.setProperty ("slot", s, nullptr);
                link.setProperty ("param", i, nullptr);
                link.setProperty ("macro", l.macro.load(), nullptr);
                link.setProperty ("lo", l.lo.load(), nullptr);
                link.setProperty ("hi", l.hi.load(), nullptr);
                macros.appendChild (link, nullptr);
            }
        }
    state.appendChild (macros, nullptr);
    return state;
}

void MultiRackProcessor::setFullState (const juce::ValueTree& stateIn)
{
    if (! stateIn.isValid() || ! stateIn.hasType (apvts.state.getType()))
        return;

    auto state = stateIn.createCopy();
    auto macros = state.getChildWithName ("MACROS");
    if (macros.isValid()) state.removeChild (macros, nullptr);

    // Mute slots whose module changes until the new type is in place.
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto p = state.getChildWithProperty ("id", slotParamId (s, "type"));
        if (p.isValid())
        {
            const int t = juce::jlimit (0, numModuleTypes() - 1, juce::roundToInt ((float) p["value"]));
            if (t != getSlotType (s))
            {
                pendingType[(size_t) s].store (t);
                slotTypeCache[(size_t) s].store (t);
            }
        }
    }

    for (auto& row : links)
        for (auto& l : row)
            l.macro.store (-1);

    apvts.replaceState (state);

    for (int s = 0; s < kNumSlots; ++s)
    {
        slotTypeCache[(size_t) s].store (typeParams[(size_t) s]->getIndex());
        if (pendingType[(size_t) s].load() == typeParams[(size_t) s]->getIndex())
            pendingType[(size_t) s].store (-1);
    }

    if (macros.isValid())
    {
        for (int m = 0; m < kNumMacros; ++m)
            macroNames[(size_t) m] = macros.getProperty ("name" + juce::String (m), "Macro " + juce::String (m + 1)).toString();

        for (auto link : macros)
        {
            const int s = link["slot"], i = link["param"], m = link["macro"];
            if (juce::isPositiveAndBelow (s, kNumSlots) && juce::isPositiveAndBelow (i, kParamsPerSlot)
                && juce::isPositiveAndBelow (m, kNumMacros))
            {
                auto& l = links[(size_t) s][(size_t) i];
                l.lo.store ((float) link["lo"]);
                l.hi.store ((float) link["hi"]);
                l.macro.store (m);
            }
        }
    }
}

void MultiRackProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! rp->getParameterID().endsWith ("_type"))
                setParamNotifying (rp, rp->getDefaultValue());

    for (auto& row : links)
        for (auto& l : row)
            l.macro.store (-1);

    for (int m = 0; m < kNumMacros; ++m)
        macroNames[(size_t) m] = "Macro " + juce::String (m + 1);
}

void MultiRackProcessor::applyPreset (const FactoryPreset& preset)
{
    resetToDefaults();

    for (int s = 0; s < kNumSlots; ++s)
    {
        if (s < (int) preset.slots.size())
        {
            const auto& fs = preset.slots[(size_t) s];
            const int type = findModuleById (fs.module);
            const auto& info = getModules()[(size_t) type];
            setSlotModule (s, type);
            for (auto& [index, value] : fs.values)
                if (index < (int) info.params.size())
                    setParamNotifying (knobParams[(size_t) s][(size_t) index], info.params[(size_t) index].toNorm (value));
        }
        else
        {
            setSlotModule (s, 0);
        }
    }
}

//==============================================================================
void MultiRackProcessor::pushUndoState (const juce::ValueTree& t)
{
    if (! undoStack.empty() && undoStack.back().isEquivalentTo (t))
        return;
    undoStack.push_back (t);
    if (undoStack.size() > 64)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void MultiRackProcessor::pushUndo() { pushUndoState (getFullState()); }

void MultiRackProcessor::beginGesture()
{
    if (gestureDepth++ == 0)
        gestureStart = getFullState();
}

void MultiRackProcessor::endGesture()
{
    if (gestureDepth == 0) return;
    if (--gestureDepth == 0 && gestureStart.isValid())
    {
        if (! getFullState().isEquivalentTo (gestureStart))
            pushUndoState (gestureStart);
        gestureStart = {};
    }
}

bool MultiRackProcessor::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back (getFullState());
    auto t = undoStack.back();
    undoStack.pop_back();
    setFullState (t);
    return true;
}

bool MultiRackProcessor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back (getFullState());
    auto t = redoStack.back();
    redoStack.pop_back();
    setFullState (t);
    return true;
}

void MultiRackProcessor::selectAB (int which)
{
    if (which == currentAB) return;
    abStates[currentAB] = getFullState();
    currentAB = which;
    if (abStates[currentAB].isValid())
        setFullState (abStates[currentAB]);
    else
        abStates[currentAB] = getFullState();
}

void MultiRackProcessor::copyToOtherAB()
{
    abStates[1 - currentAB] = getFullState();
}

//==============================================================================
void MultiRackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("MULTIRACK");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("ab", currentAB, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.setProperty ("preset", presets.currentName, nullptr);

    juce::ValueTree cur ("CURRENT");
    cur.appendChild (getFullState(), nullptr);
    root.appendChild (cur, nullptr);

    const auto& other = abStates[1 - currentAB];
    if (other.isValid())
    {
        juce::ValueTree o ("OTHER");
        o.appendChild (other.createCopy(), nullptr);
        root.appendChild (o, nullptr);
    }

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void MultiRackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("MULTIRACK")) return;

    currentAB = juce::jlimit (0, 1, (int) root.getProperty ("ab", 0));
    uiScale = juce::jlimit (0.5f, 2.0f, (float) root.getProperty ("uiScale", 1.0f));
    presets.currentName = root.getProperty ("preset", "Default").toString();

    auto cur = root.getChildWithName ("CURRENT");
    if (cur.isValid() && cur.getNumChildren() > 0)
        setFullState (cur.getChild (0));

    abStates[currentAB] = {};
    abStates[1 - currentAB] = {};
    auto other = root.getChildWithName ("OTHER");
    if (other.isValid() && other.getNumChildren() > 0)
        abStates[1 - currentAB] = other.getChild (0).createCopy();

    undoStack.clear();
    redoStack.clear();
}

//==============================================================================
juce::AudioProcessorEditor* MultiRackProcessor::createEditor()
{
    return new MultiRackEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiRackProcessor();
}
