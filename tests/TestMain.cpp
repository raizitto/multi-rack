// Offline test harness for Multi Rack: exercises every module, presets, HQ mode, state, undo and renders the UI.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mr;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "  FAIL: " << msg << std::endl; } } while (0)

static void fillSignal (juce::AudioBuffer<float>& b, juce::Random& rng, float level, int offset)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = (float) (offset + i) / 48000.0f;
        const float s = level * (0.6f * std::sin (2.0f * juce::MathConstants<float>::pi * 110.0f * t)
                                 + 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 3150.0f * t)
                                 + 0.15f * (rng.nextFloat() * 2.0f - 1.0f));
        b.setSample (0, i, s);
        b.setSample (1, i, s * 0.9f);
        for (int c = 2; c < b.getNumChannels(); ++c) b.setSample (c, i, 0.0f);
    }
}

struct RunStats { float peak = 0; double sumSq = 0; long n = 0; bool finite = true; };

static RunStats run (MultiRackProcessor& p, int blocks, float level, juce::Random& rng, int blockSize = 512)
{
    juce::AudioBuffer<float> buf (4, blockSize);
    juce::MidiBuffer midi;
    RunStats st;
    for (int b = 0; b < blocks; ++b)
    {
        fillSignal (buf, rng, level, b * blockSize);
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) st.finite = false;
                st.peak = std::max (st.peak, std::abs (v));
                st.sumSq += (double) v * v; ++st.n;
            }
    }
    return st;
}

static void setParam (MultiRackProcessor& p, const juce::String& id, float norm)
{
    p.apvts.getParameter (id)->setValueNotifyingHost (norm);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Random rng (1234);

    MultiRackProcessor proc;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    CHECK (proc.setBusesLayout (layout), "stereo + sidechain layout");
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    //== 1. Null test: empty rack is bit transparent ==========================
    std::cout << "[1] empty rack transparency" << std::endl;
    proc.clearRack();
    {
        juce::AudioBuffer<float> buf (4, 512), ref (4, 512);
        juce::MidiBuffer midi;
        float maxErr = 0;
        for (int b = 0; b < 20; ++b)
        {
            fillSignal (buf, rng, 0.5f, b * 512);
            ref.makeCopyOf (buf);
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    maxErr = std::max (maxErr, std::abs (buf.getSample (c, i) - ref.getSample (c, i)));
        }
        std::cout << "  max error " << maxErr << std::endl;
        CHECK (maxErr < 1.0e-6f, "empty rack should be transparent");
    }

    //== 2. Every module, random parameters, normal + HQ ======================
    for (int hq = 0; hq < 2; ++hq)
    {
        setParam (proc, "hq", (float) hq);
        run (proc, 2, 0.1f, rng);
        std::cout << "[2] all modules, random params, HQ=" << hq << " latency=" << proc.getLatencySamples() << std::endl;
        if (hq) CHECK (proc.getLatencySamples() > 0, "HQ should report latency");
        else    CHECK (proc.getLatencySamples() == 0, "normal mode has no latency");

        for (int t = 1; t < numModuleTypes(); ++t)
        {
            proc.clearRack();
            proc.setSlotModule (0, t);
            const auto& info = getModules()[(size_t) t];

            // defaults (warm up first so filter state left by the previous module's random test is flushed)
            run (proc, 4, 0.0f, rng);
            auto st = run (proc, 40, 0.3f, rng);
            const float rmsDb = (float) (10.0 * std::log10 (st.sumSq / (double) st.n + 1e-20));
            std::cout << "  " << info.name.paddedRight (' ', 18) << " default: peak " << juce::String (st.peak, 3)
                      << "  rms " << juce::String (rmsDb, 1) << " dB" << std::endl;
            CHECK (st.finite, info.name << " default produced non-finite output");
            CHECK (st.peak < 8.0f, info.name << " default output too hot");

            // random parameters, including extremes, with external sidechain present
            for (int trial = 0; trial < 25; ++trial)
            {
                for (int i = 0; i < (int) info.params.size(); ++i)
                {
                    float v = rng.nextFloat();
                    if (trial % 5 == 0) v = rng.nextBool() ? 0.0f : 1.0f;
                    setParam (proc, slotKnobId (0, i), v);
                }
                const float level = trial % 3 == 0 ? 1.5f : (trial % 3 == 1 ? 0.01f : 0.4f);
                auto r = run (proc, 6, level, rng, 256 + (trial % 4) * 300);
                CHECK (r.finite, info.name << " trial " << trial << " non-finite");
                CHECK (r.peak < 300.0f, info.name << " trial " << trial << " runaway peak " << r.peak);
            }
        }
    }
    setParam (proc, "hq", 0.0f);

    //== 3. Factory presets =====================================================
    std::cout << "[3] factory presets" << std::endl;
    for (int i = 0; i < (int) getFactoryPresets().size(); ++i)
    {
        proc.getPresets().load (i);
        auto st = run (proc, 60, 0.25f, rng);
        const float rmsDb = (float) (10.0 * std::log10 (st.sumSq / (double) st.n + 1e-20));
        std::cout << "  " << proc.getPresets().currentName.paddedRight (' ', 20) << " peak " << juce::String (st.peak, 3)
                  << "  rms " << juce::String (rmsDb, 1) << " dB" << std::endl;
        CHECK (st.finite && st.peak < 4.0f, "preset " << proc.getPresets().currentName);
    }

    //== 4. Swap / move keeps parameter values ===============================
    std::cout << "[4] move slots" << std::endl;
    proc.clearRack();
    proc.setSlotModule (0, findModuleById ("quadeq"));
    proc.setSlotModule (1, findModuleById ("limiter"));
    setParam (proc, slotKnobId (0, 0), 0.9f);
    proc.assignMacro (0, 0, 2);
    proc.moveSlot (0, 3);
    CHECK (proc.getSlotType (3) == findModuleById ("quadeq"), "module moved to D");
    CHECK (proc.getSlotType (0) == findModuleById ("limiter"), "limiter moved to A");
    CHECK (std::abs (proc.getKnobParam (3, 0)->getValue() - 0.9f) < 1e-4f, "param followed module");
    CHECK (proc.getLink (3, 0).macro.load() == 2, "macro link followed module");
    run (proc, 4, 0.3f, rng);

    //== 5. State round trip ==================================================
    std::cout << "[5] state save/restore" << std::endl;
    proc.setMacroName (2, "Punch");
    juce::MemoryBlock mb;
    proc.getStateInformation (mb);
    proc.clearRack();
    proc.setMacroName (2, "x");
    proc.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (proc.getSlotType (3) == findModuleById ("quadeq"), "restored module");
    CHECK (std::abs (proc.getKnobParam (3, 0)->getValue() - 0.9f) < 1e-4f, "restored param");
    CHECK (proc.getLink (3, 0).macro.load() == 2, "restored macro link");
    CHECK (proc.getMacroName (2) == "Punch", "restored macro name");

    //== 6. Undo / redo / A-B ==================================================
    std::cout << "[6] undo/redo, A/B" << std::endl;
    proc.pushUndo();
    proc.setSlotModule (5, findModuleById ("tape"));
    CHECK (proc.getSlotType (5) == findModuleById ("tape"), "tape added");
    proc.undo();
    CHECK (proc.getSlotType (5) == 0, "undo removed tape");
    proc.redo();
    CHECK (proc.getSlotType (5) == findModuleById ("tape"), "redo re-added tape");
    proc.selectAB (1);
    proc.clearRack();
    proc.selectAB (0);
    CHECK (proc.getSlotType (5) == findModuleById ("tape"), "A kept its rack");
    proc.selectAB (1);
    CHECK (proc.getSlotType (5) == 0, "B is the cleared rack");
    proc.selectAB (0);

    //== 7. Macro drives parameter ============================================
    std::cout << "[7] macros" << std::endl;
    proc.clearRack();
    proc.setSlotModule (0, findModuleById ("trimmer"));
    proc.assignMacro (0, 0, 0);          // gain
    proc.setMacroRange (0, 0, 0.5f, 0.0f); // 0 dB -> -24 dB
    setParam (proc, macroId (0), 1.0f);
    {
        auto st = run (proc, 20, 0.5f, rng);
        std::cout << "  macro at 100% -> peak " << st.peak << std::endl;
        CHECK (st.peak < 0.05f, "macro should pull trimmer gain to -24 dB");
    }
    setParam (proc, macroId (0), 0.0f);

    //== 8. Mono layout ========================================================
    std::cout << "[8] mono layout" << std::endl;
    {
        MultiRackProcessor mono;
        juce::AudioProcessor::BusesLayout ml;
        ml.inputBuses.add (juce::AudioChannelSet::mono());
        ml.inputBuses.add (juce::AudioChannelSet::disabled());
        ml.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (mono.setBusesLayout (ml), "mono layout accepted");
        mono.prepareToPlay (44100.0, 128);
        juce::AudioBuffer<float> b (1, 128);
        juce::MidiBuffer midi;
        for (int k = 0; k < 50; ++k)
        {
            for (int i = 0; i < 128; ++i) b.setSample (0, i, 0.3f * std::sin ((float) (k * 128 + i) * 0.05f));
            mono.processBlock (b, midi);
        }
        CHECK (std::isfinite (b.getSample (0, 10)), "mono finite");
    }

    //== 9. Render the editor ==================================================
    std::cout << "[9] render UI" << std::endl;
    proc.getPresets().load (2); // Vocal Strip
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (MultiRackEditor::kBaseW, MultiRackEditor::kBaseH);
        run (proc, 30, 0.4f, rng);
        for (int i = 0; i < 10; ++i)
        {
            run (proc, 2, 0.4f, rng);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const auto out = juce::File (argc > 1 ? argv[1] : "multirack_ui.png");
        out.deleteFile();
        juce::FileOutputStream fos (out);
        juce::PNGImageFormat png;
        CHECK (png.writeImageToStream (img, fos), "write png");
        std::cout << "  wrote " << out.getFullPathName() << std::endl;
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
