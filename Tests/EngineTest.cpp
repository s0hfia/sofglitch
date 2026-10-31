#include "../Source/PluginProcessor.h"
#include <cstdio>

struct FakeHead : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (true);
        p.setTimeSignature (TimeSignature { 4, 4 }); return p;
    }
};

static int runCase (int place)
{
    juce::ScopedJuceInitialiser_GUI init;
    SofGlitchProcessor proc;
    FakeHead head;
    const double sr = 48000; const int block = 256;
    proc.setPlayHead (&head);
    proc.prepareToPlay (sr, block);
    // isolate lane 1: Cut on last beat of 16 beats; disable others
    for (int i = 1; i < sg::kNumLanes; ++i) proc.apvts.getParameter (sg::laneParam (i, "on"))->setValueNotifyingHost (0.0f);
    proc.apvts.getParameter (sg::laneParam (0, "place"))->setValueNotifyingHost ((float) place);

    juce::Random rng (1);
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    const double spb = 60.0 * sr / head.bpm;
    const int totalBeats = 32;
    std::vector<double> rmsPerBeat (totalBeats, 0.0); std::vector<int> cnt (totalBeats, 0);
    double samplePos = 0;
    while (samplePos < totalBeats * spb)
    {
        head.ppq = samplePos / spb;
        for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < block; ++n) buf.setSample (ch, n, rng.nextFloat() * 2 - 1);
        proc.processBlock (buf, midi);
        for (int n = 0; n < block; ++n)
        {
            int beat = (int) ((samplePos + n) / spb); if (beat >= totalBeats) break;
            // skip 3ms at beat edges (fades)
            double inBeat = std::fmod (samplePos + n, spb);
            if (inBeat < 0.003 * sr || inBeat > spb - 0.003 * sr) continue;
            float s = buf.getSample (0, n); rmsPerBeat[beat] += s * s; cnt[beat]++;
        }
        samplePos += block;
    }
    int fails = 0;
    for (int b = 0; b < totalBeats; ++b)
    {
        double rms = std::sqrt (rmsPerBeat[b] / juce::jmax (1, cnt[b]));
        bool shouldCut = place == 0 ? (b % 16) == 15 : (b % 16) == 0;
        bool isSilent = rms < 1e-4;
        printf ("beat %2d (bar %d.%d): rms=%.3f %s\n", b, b / 4 + 1, b % 4 + 1, rms, shouldCut ? "<-- expected CUT" : "");
        if (shouldCut != isSilent) fails++;
    }
    printf (fails ? "FAIL (%d)\n" : "OK\n", fails);
    return fails;
}

static int actionSanity()
{
    int fails = 0;
    for (int a = 0; a < sg::NumActions; ++a)
    {
        SofGlitchProcessor proc; FakeHead head; const double sr = 48000; const int block = 256;
        proc.setPlayHead (&head); proc.prepareToPlay (sr, block);
        for (int i = 1; i < sg::kNumLanes; ++i) proc.apvts.getParameter (sg::laneParam (i, "on"))->setValueNotifyingHost (0.0f);
        auto* act = proc.apvts.getParameter (sg::laneParam (0, "action"));
        act->setValueNotifyingHost (act->convertTo0to1 ((float) a));
        proc.apvts.getParameter (sg::laneParam (0, "p1"))->setValueNotifyingHost (0.7f);
        proc.apvts.getParameter (sg::laneParam (0, "p2"))->setValueNotifyingHost (0.6f);
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
        const double spb = 60.0 * sr / head.bpm; double pos = 0; float peak = 0; bool finite = true; double ph = 0;
        while (pos < 20 * spb)
        {
            head.ppq = pos / spb;
            for (int n = 0; n < block; ++n) { float v = 0.5f * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * 220.0 / sr; buf.setSample (0, n, v); buf.setSample (1, n, v); }
            proc.processBlock (buf, midi);
            for (int n = 0; n < block; ++n) { float v = buf.getSample (0, n); if (! std::isfinite (v)) finite = false; peak = juce::jmax (peak, std::abs (v)); }
            pos += block;
        }
        bool ok = finite && peak < 2.0f;
        printf ("action %-13s peak=%.2f %s\n", sg::actionNames()[a].toRawUTF8(), peak, ok ? "ok" : "BAD");
        if (! ok) fails++;
    }
    return fails;
}

static int chainSanity()
{
    SofGlitchProcessor proc; FakeHead head; const double sr = 48000; const int block = 256;
    proc.setPlayHead (&head); proc.prepareToPlay (sr, block);
    for (int i = 1; i < sg::kNumLanes; ++i) proc.apvts.getParameter (sg::laneParam (i, "on"))->setValueNotifyingHost (0.0f);
    auto set = [&proc] (const char* n, float norm) { proc.apvts.getParameter (sg::laneParam (0, n))->setValueNotifyingHost (norm); };
    auto* a0 = proc.apvts.getParameter (sg::laneParam (0, "action"));
    a0->setValueNotifyingHost (a0->convertTo0to1 ((float) sg::Stutter));
    auto* ab = proc.apvts.getParameter (sg::laneParam (0, "action_b"));
    ab->setValueNotifyingHost (ab->convertTo0to1 ((float) sg::Crush + 1));
    auto* ac = proc.apvts.getParameter (sg::laneParam (0, "action_c"));
    ac->setValueNotifyingHost (ac->convertTo0to1 ((float) sg::Filter + 1));
    set ("p1_b", 0.8f); set ("p1_c", 0.9f); set ("p2_c", 0.1f);
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    const double spb = 60.0 * sr / head.bpm; double pos = 0; bool finite = true; float peak = 0; double ph = 0;
    while (pos < 20 * spb)
    {
        head.ppq = pos / spb;
        for (int n = 0; n < block; ++n) { float v = 0.5f * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * 220.0 / sr; buf.setSample (0, n, v); buf.setSample (1, n, v); }
        proc.processBlock (buf, midi);
        for (int n = 0; n < block; ++n) { float v = buf.getSample (0, n); if (! std::isfinite (v)) finite = false; peak = juce::jmax (peak, std::abs (v)); }
        pos += block;
    }
    const bool ok = finite && peak < 2.0f;
    printf ("chain Stutter>Crush>Filter peak=%.2f %s\n", peak, ok ? "ok" : "BAD");
    return ok ? 0 : 1;
}

static int tailSanity()
{
    // Delay Throw on the last beat of 16; input goes silent right after the window.
    // If the throw works, the delay tail must still be audible in the following beats.
    SofGlitchProcessor proc; FakeHead head; const double sr = 48000; const int block = 256;
    proc.setPlayHead (&head); proc.prepareToPlay (sr, block);
    for (int i = 1; i < sg::kNumLanes; ++i) proc.apvts.getParameter (sg::laneParam (i, "on"))->setValueNotifyingHost (0.0f);
    auto* a0 = proc.apvts.getParameter (sg::laneParam (0, "action"));
    a0->setValueNotifyingHost (a0->convertTo0to1 ((float) sg::DelayThrow));
    auto set = [&proc] (const char* n, float v) { proc.apvts.getParameter (sg::laneParam (0, n))->setValueNotifyingHost (v); };
    set ("p1", 0.4f); set ("p2", 0.7f); set ("p3", 0.2f); set ("p4", 0.8f);
    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    const double spb = 60.0 * sr / head.bpm; double pos = 0; double ph = 0;
    double tailEnergy = 0;
    while (pos < 18 * spb)
    {
        head.ppq = pos / spb;
        const bool silent = pos >= 16 * spb;
        for (int n = 0; n < block; ++n)
        {
            float v = silent ? 0.0f : 0.5f * (float) std::sin (ph);
            ph += 2 * juce::MathConstants<double>::pi * 220.0 / sr;
            buf.setSample (0, n, v); buf.setSample (1, n, v);
        }
        proc.processBlock (buf, midi);
        if (pos >= 16.5 * spb)
            for (int n = 0; n < block; ++n) tailEnergy += std::abs (buf.getSample (0, n));
        pos += block;
    }
    const bool ok = tailEnergy > 1.0;
    printf ("delay throw tail energy=%.1f %s\n", tailEnergy, ok ? "ok (rings past the one)" : "BAD (no tail)");
    return ok ? 0 : 1;
}

int main()
{
    printf ("--- BEFORE the one\n"); int f = runCase (0);
    printf ("--- ON the one\n"); f += runCase (1);
    printf ("--- action sanity\n"); f += actionSanity();
    printf ("--- chain sanity\n"); f += chainSanity();
    printf ("--- throw tail\n"); f += tailSanity();
    printf (f ? "FAIL\n" : "ALL GOOD\n"); return f;
}
