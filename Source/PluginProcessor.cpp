#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using namespace sg;

// ---------------------------------------------------------------- parameters
juce::AudioProcessorValueTreeState::ParameterLayout SofGlitchProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    using I = juce::AudioParameterInt;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P> (juce::ParameterID { "mix", 1 }, "Mix", 0.0f, 1.0f, 1.0f));
    layout.add (std::make_unique<I> (juce::ParameterID { "seed", 1 }, "Seed", 0, 999, 1));
    layout.add (std::make_unique<I> (juce::ParameterID { "phase", 1 }, "Phrase Offset (bars)", 0, 15, 0));
    layout.add (std::make_unique<B> (juce::ParameterID { "freerun", 1 }, "Free Run", false));
    layout.add (std::make_unique<P> (juce::ParameterID { "intbpm", 1 }, "Internal BPM", 40.0f, 240.0f, 140.0f));

    // sensible fractal defaults: phrase cut / bar-end stutter / beat-end ramp / 32nd reverse / sample slot
    struct Def { bool en; int unit; int group; int count; int every; int action; float len; float prob; float p1; float p2; };
    const Def defs[kNumLanes] = {
        { true,  2, 16, 1, 1, Cut,         1.0f, 1.0f,  0.5f, 0.5f },   // last beat of every 4 bars (16 quarter notes)
        { true,  4, 16, 1, 1, Stutter,     1.0f, 0.75f, 0.55f, 0.2f },  // last 16th of every bar
        { true,  5,  8, 1, 2, StutterRamp, 1.0f, 0.5f,  0.4f, 0.6f },   // last 32nd of every other beat
        { false, 6, 16, 1, 1, Reverse,     1.0f, 0.4f,  0.5f, 0.5f },   // last 64th of each beat
        { false, 2, 32, 1, 1, Sample,      1.0f, 1.0f,  0.6f, 0.3f },   // last beat of every 8 bars: sample hit
    };

    for (int i = 0; i < kNumLanes; ++i)
    {
        const auto& d = defs[i];
        auto id = [i] (const char* n) { return juce::ParameterID { laneParam (i, n), 1 }; };
        auto nm = [i] (const char* n) { return "L" + juce::String (i + 1) + " " + n; };
        layout.add (std::make_unique<B> (id ("on"), nm ("On"), d.en));
        layout.add (std::make_unique<C> (id ("unit"), nm ("Unit"), unitNames(), d.unit));
        layout.add (std::make_unique<I> (id ("group"), nm ("Group"), 2, 64, d.group));
        layout.add (std::make_unique<I> (id ("count"), nm ("Count"), 1, 16, d.count));
        layout.add (std::make_unique<I> (id ("every"), nm ("Every"), 1, 16, d.every));
        layout.add (std::make_unique<C> (id ("action"), nm ("Action"), actionNames(), d.action));
        layout.add (std::make_unique<P> (id ("len"), nm ("Length"), 0.05f, 1.0f, d.len));
        layout.add (std::make_unique<P> (id ("prob"), nm ("Prob"), 0.0f, 1.0f, d.prob));
        layout.add (std::make_unique<P> (id ("p1"), nm ("P1"), 0.0f, 1.0f, d.p1));
        layout.add (std::make_unique<P> (id ("p2"), nm ("P2"), 0.0f, 1.0f, d.p2));
        layout.add (std::make_unique<P> (id ("p3"), nm ("P3"), 0.0f, 1.0f, 0.5f));
        layout.add (std::make_unique<P> (id ("p4"), nm ("P4"), 0.0f, 1.0f, 0.0f));
        for (auto* sl : { "b", "c" })
        {
            auto sid = [i, sl] (const char* n) { return juce::ParameterID { laneParam (i, n) + "_" + sl, 1 }; };
            auto snm = [i, sl] (const char* n) { return "L" + juce::String (i + 1) + " " + juce::String (sl).toUpperCase() + " " + n; };
            layout.add (std::make_unique<C> (sid ("action"), snm ("Action"), actionNamesWithOff(), 0));
            layout.add (std::make_unique<P> (sid ("p1"), snm ("P1"), 0.0f, 1.0f, 0.5f));
            layout.add (std::make_unique<P> (sid ("p2"), snm ("P2"), 0.0f, 1.0f, 0.5f));
            layout.add (std::make_unique<P> (sid ("p3"), snm ("P3"), 0.0f, 1.0f, 0.5f));
            layout.add (std::make_unique<P> (sid ("p4"), snm ("P4"), 0.0f, 1.0f, 0.0f));
        }
        layout.add (std::make_unique<P> (id ("mix"), nm ("Mix"), 0.0f, 1.0f, 1.0f));
        layout.add (std::make_unique<P> (id ("gain"), nm ("Gain dB"), -6.0f, 18.0f, 0.0f));
        layout.add (std::make_unique<C> (id ("place"), nm ("Place"), placeNames(), 0));
        layout.add (std::make_unique<P> (id ("nudge"), nm ("Nudge"), -32.0f, 32.0f, 0.0f));
        layout.add (std::make_unique<C> (id ("mode"), nm ("Mode"), modeNames(), 0));
        layout.add (std::make_unique<I> (id ("clen"), nm ("Pattern Length"), 1, 64, 16));
        layout.add (std::make_unique<P> (id ("send"), nm ("Sample End"), 0.0f, 1.0f, 1.0f));
        layout.add (std::make_unique<I> (id ("spitch"), nm ("Sample Pitch"), -24, 24, 0));
        layout.add (std::make_unique<B> (id ("srev"), nm ("Sample Reverse"), false));
        layout.add (std::make_unique<B> (id ("sfit"), nm ("Sample Fit"), false));
        layout.add (std::make_unique<P> (id ("sstart"), nm ("Sample Start"), 0.0f, 1.0f, 0.0f));
    }
    return layout;
}

SofGlitchProcessor::SofGlitchProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
    for (int i = 0; i < kNumLanes; ++i)
    {
        auto g = [this, i] (const juce::String& n) { return apvts.getRawParameterValue (laneParam (i, n.toRawUTF8())); };
        auto& P = lp[(size_t) i];
        P.enabled = g ("on"); P.unit = g ("unit"); P.group = g ("group"); P.count = g ("count"); P.every = g ("every");
        P.length = g ("len"); P.prob = g ("prob"); P.mix = g ("mix"); P.gain = g ("gain");
        P.place = g ("place"); P.nudge = g ("nudge"); P.mode = g ("mode"); P.clen = g ("clen"); P.send = g ("send");
        P.spitch = g ("spitch"); P.srev = g ("srev"); P.sfit = g ("sfit"); P.sstart = g ("sstart");
        const char* sfx[kNumSlots] = { "", "_b", "_c" };
        for (int k = 0; k < kNumSlots; ++k)
        {
            P.action[k] = g (juce::String ("action") + sfx[k]);
            P.p1[k] = g (juce::String ("p1") + sfx[k]); P.p2[k] = g (juce::String ("p2") + sfx[k]);
            P.p3[k] = g (juce::String ("p3") + sfx[k]); P.p4[k] = g (juce::String ("p4") + sfx[k]);
        }
        laneActivity[i] = 0.0f;
        for (auto& w : customBits[(size_t) i]) w = 0;
    }
    pMix = apvts.getRawParameterValue ("mix");
    pSeed = apvts.getRawParameterValue ("seed");
    pPhase = apvts.getRawParameterValue ("phase");
    pFreeRun = apvts.getRawParameterValue ("freerun");
    pIntBpm = apvts.getRawParameterValue ("intbpm");
}

void SofGlitchProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    histSize = (int) (sampleRate * 10.0);
    history.setSize (2, histSize);
    history.clear();
    writePos = 0;
    internalPpq = 0.0;
    juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
    for (auto& L : lanes)
    {
        L.active = false; L.lastCycle = -1;
        for (auto& S : L.slots)
        {
            S.filter.prepare (spec);
            S.filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            S.filter.setResonance (0.9f);
            for (int ch = 0; ch < 2; ++ch)
            {
                S.sgFifo[ch].assign (kFFTSize, 0.0f);
                S.sgOut[ch].assign (kFFTSize, 0.0f);
                S.dline[ch].assign ((size_t) (sampleRate * 1.0) + 8, 0.0f);
                S.dampZ[ch] = 0; S.throwFeed[ch] = 0;
            }
            S.sgGain.assign (kFFTSize / 2 + 1, 0.0f);
            S.sgIdx = 0; S.sgHopCount = 0; S.dlWrite = 0; S.rmPhase = 0;
            S.reverb.setSampleRate (sampleRate);
            S.reverb.reset();
        }
    }
    hannWin.resize (kFFTSize);
    for (int n = 0; n < kFFTSize; ++n)
        hannWin[(size_t) n] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * n / kFFTSize);
}

bool SofGlitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

// ---------------------------------------------------------------- helpers
inline float SofGlitchProcessor::readHist (int ch, double absPos) const
{
    // never read the future
    const double maxPos = (double) writePos;
    if (absPos > maxPos) absPos = maxPos;
    if (absPos < 0) return 0.0f;
    const auto i0 = (int64_t) absPos;
    const float frac = (float) (absPos - (double) i0);
    const int a = (int) (i0 % histSize);
    const int b = (int) ((i0 + 1) % histSize);
    const float* d = history.getReadPointer (ch);
    return d[a] + (d[b] - d[a]) * frac;
}


static inline int sliceDivFromP (float p)   // 0..1 -> slice length as a fraction of a beat
{
    // 1/8, 1/16, 1/32, 1/64, 1/128, 1/256 notes
    return 1 << (1 + juce::jlimit (0, 5, (int) std::floor (p * 5.999f)));   // returns 2..64 divisions of a beat
}

// Parker (DAFx-11) diode ring modulator nonlinearity, as used by Surge XT (vb=0.2, vl=0.4)
static inline float diodeShape (float v)
{
    constexpr float vb = 0.2f, vl = 0.4f, h = 1.0f;
    if (v <= vb) return 0.0f;
    if (v <= vl) { const float d = v - vb; return h * d * d / (2.0f * (vl - vb)); }
    return h * (v - (vl + vb) * 0.5f);
}

// One STFT frame of the spectral gate: per-bin threshold, smoothed gains, shared across channels
void SofGlitchProcessor::spectralFrame (SlotState& S)
{
    const int half = kFFTSize / 2;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* w = fftWork[ch];
        std::fill (w, w + kFFTSize * 2, 0.0f);
        for (int n = 0; n < kFFTSize; ++n)
            w[n] = S.sgFifo[ch][(size_t) ((S.sgIdx + n) % kFFTSize)] * hannWin[(size_t) n];
        fft.performRealOnlyForwardTransform (w, true);
    }
    const float thrAmp = juce::Decibels::decibelsToGain (-72.0f + 66.0f * S.p1) * (float) kFFTSize * 0.25f;
    const float floorGain = juce::Decibels::decibelsToGain (-6.0f - 54.0f * S.p2);
    const float rel = 0.1f + 0.85f * S.p3;          // per-frame gain smoothing (release)
    const float inv = S.p4;
    for (int b = 0; b <= half; ++b)
    {
        const float re = fftWork[0][b * 2], im = fftWork[0][b * 2 + 1];
        const float re2 = fftWork[1][b * 2], im2 = fftWork[1][b * 2 + 1];
        const float mag = 0.5f * (std::sqrt (re * re + im * im) + std::sqrt (re2 * re2 + im2 * im2));
        const bool open = mag > thrAmp;
        const float tOpen = open ? 1.0f : floorGain;
        const float target = tOpen * (1.0f - inv) + (open ? floorGain : 1.0f) * inv;
        auto& g = S.sgGain[(size_t) b];
        g = target > g ? target : target + (g - target) * rel;   // fast attack, smoothed release
        for (int ch = 0; ch < 2; ++ch) { fftWork[ch][b * 2] *= g; fftWork[ch][b * 2 + 1] *= g; }
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        fft.performRealOnlyInverseTransform (fftWork[ch]);
        for (int n = 0; n < kFFTSize; ++n)
        {
            auto& o = S.sgOut[ch][(size_t) ((S.sgIdx + n) % kFFTSize)];
            o += fftWork[ch][n] * hannWin[(size_t) n] * (1.0f / 1.5f);   // Hann^2 @ 75% overlap sums to 1.5
        }
    }
}

// ---------------------------------------------------------------- the actions
// Renders one fx slot. inL/inR is the chain signal from the previous slot: transformers
// (Cut/Duck/Gate/Filter/Crush) process it; generators resynthesize from the history buffer
// and use it as their "dry under" where they have a dry-blend parameter.
void SofGlitchProcessor::renderSlot (LaneState& L, SlotState& S, float inL, float inR, float& outL, float& outR)
{
    const double t = (double) L.t;
    const double len = (double) juce::jmax<int64_t> (1, L.eventLen);
    const double spb = 60.0 * sr / displayBpm.load();   // samples per beat
    const double es = (double) L.eventStart;
    outL = inL; outR = inR;

    switch (S.action)
    {
        case Cut: outL = 0; outR = 0; break;

        case Duck:
        {
            const float g = juce::Decibels::decibelsToGain (-60.0f * S.p1);
            outL = inL * g; outR = inR * g; break;
        }
        case Gate:
        {
            const double period = spb / sliceDivFromP (S.p1);
            const double duty = 0.1 + 0.8 * S.p2;
            const double ph = std::fmod (t, period) / period;
            const float g = ph < duty ? 1.0f : 0.0f;
            outL = inL * g; outR = inR * g; break;
        }
        case Filter:
        {
            if ((L.t & 15) == 0)
            {
                const float f0 = 40.0f * std::pow (450.0f, S.p1);   // 40 .. 18k (log)
                const float f1 = 40.0f * std::pow (450.0f, S.p2);
                const float f = f0 * std::pow (f1 / f0, (float) (t / len));
                S.filter.setCutoffFrequency (juce::jlimit (30.0f, (float) sr * 0.45f, f));
            }
            outL = S.filter.processSample (0, inL);
            outR = S.filter.processSample (1, inR);
            break;
        }
        case Crush:
        {
            const float bits = 16.0f - 14.0f * S.p1;
            const float step = 2.0f / std::pow (2.0f, bits);
            const int down = 1 + (int) (S.p2 * 40.0f);
            if (S.crushCount++ % down == 0) { S.crushHold[0] = inL; S.crushHold[1] = inR; }
            outL = std::round (S.crushHold[0] / step) * step;
            outR = std::round (S.crushHold[1] / step) * step;
            break;
        }
        case Stutter:
        {
            const double slice = juce::jmax (32.0, spb / sliceDivFromP (S.p1));
            const double k = std::floor (t / slice);
            const float decay = std::pow (1.0f - 0.5f * S.p2, (float) k);
            const double pos = es + std::fmod (t, slice);
            outL = readHist (0, pos) * decay; outR = readHist (1, pos) * decay;
            break;
        }
        case StutterRamp:
        {
            const int stages = 2 + (int) std::round (S.p2 * 4.0f);     // 2..6 halvings
            const int stage = juce::jmin (stages - 1, (int) (t / len * stages));
            const double slice = juce::jmax (32.0, spb / sliceDivFromP (S.p1) / (double) (1 << stage));
            const double pos = es + std::fmod (t, slice);
            outL = readHist (0, pos); outR = readHist (1, pos);
            break;
        }
        case Reverse:
        {
            const double pos = es - t;
            outL = readHist (0, pos); outR = readHist (1, pos);
            break;
        }
        case Hold:
        {
            const double grain = juce::jmax (16.0, (2.0 + 98.0 * S.p1) * 0.001 * sr);
            const double ph = std::fmod (t, grain);
            const double fade = grain * 0.15;
            float w = 1.0f;
            if (ph < fade) w = (float) (ph / fade); else if (ph > grain - fade) w = (float) ((grain - ph) / fade);
            outL = readHist (0, es + ph) * w; outR = readHist (1, es + ph) * w;
            break;
        }
        case Pitch: case OctaveUp: case OctaveDown:
        {
            const float semis = S.action == OctaveUp ? 12.0f : S.action == OctaveDown ? -12.0f
                              : std::round ((S.p1 * 2.0f - 1.0f) * 24.0f);
            const double r = std::pow (2.0, semis / 12.0);
            const double start = r > 1.0 ? es - len * (r - 1.0) : es;
            const double pos = start + t * r;
            outL = readHist (0, pos); outR = readHist (1, pos);
            break;
        }
        case PitchDive:
        {
            const double semisDown = 3.0 + 45.0 * S.p1;
            const double r = std::pow (2.0, -semisDown / 12.0 * (t / len));
            if (L.t == 0) S.readPos = es;
            outL = readHist (0, S.readPos); outR = readHist (1, S.readPos);
            S.readPos += r;
            break;
        }
        case TapeStop:
        {
            const double r = std::pow (1.0 - t / len, 1.0 + 3.0 * S.p1);
            if (L.t == 0) S.readPos = es;
            outL = readHist (0, S.readPos); outR = readHist (1, S.readPos);
            S.readPos += r;
            break;
        }
        case Stretch:
        {
            // granular time-stretch: the source pointer slows down while grains play at (almost) normal speed
            const double finalSpeed = 1.0 - S.p1;                       // 1 = full freeze at the end
            const double curve = 1.0 + 4.0 * S.p2;                      // 1 = linear, higher = fast slam then crawl
            const double prog = std::pow (t / len, 1.0 / curve);
            const double speed = 1.0 + (finalSpeed - 1.0) * prog;        // source advance per sample
            const double grain = juce::jmax (64.0, (10.0 + 90.0 * S.p3) * 0.001 * sr);
            const double drop = S.p4;                                   // 0 = pure stretch, 1 = tape-stop pitch follow
            const double playRate = 1.0 - drop * (1.0 - speed);
            if (L.t == 0) S.readPos = es;
            // two overlapping grains, each pinned to where the source pointer was when the grain began
            const double half = grain * 0.5;
            float oL = 0, oR = 0;
            for (int k = 0; k < 2; ++k)
            {
                const double ph = std::fmod (t + k * half, grain);       // position inside this grain
                const double gs = t - ph;                                // grain start time (event-relative)
                // source pointer at grain start = es + integral of speed up to gs  (closed form for power curve)
                // integral of speed(tau) = 1 + (f-1)*(tau/len)^(1/c) from 0 to gs
                const double e = 1.0 + 1.0 / curve;
                const double srcAtGs = es + gs + (finalSpeed - 1.0) * len * std::pow (juce::jmax (0.0, gs) / len, e) / e;
                const double pos = srcAtGs + ph * playRate;
                const float w = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * ph / grain));
                oL += readHist (0, pos) * w; oR += readHist (1, pos) * w;
            }
            outL = oL; outR = oR;
            break;
        }
        case Unison:
        {
            // N voices pitch-shifted by +/- cents via crossfaded delay heads, spread across the stereo field
            const double cents = 2.0 + 98.0 * S.p1;
            const int voices = 2 + (int) std::round (S.p2 * 6.0f);      // 2..8
            const float width = S.p3;
            const double W = 0.04 * sr;
            const double now = (double) (L.eventStart + L.t);
            float oL = 0, oR = 0;
            for (int v = 0; v < voices; ++v)
            {
                const double frac = voices > 1 ? (double) v / (voices - 1) * 2.0 - 1.0 : 0.0;   // -1..1
                const double r = std::pow (2.0, frac * cents / 1200.0);
                const double phi = t * (1.0 - r) / W;
                float mono = 0;
                for (int k = 0; k < 2; ++k)
                {
                    double f = phi + k * 0.5; f -= std::floor (f);
                    const double d = W * f;
                    const float g = (float) std::sin (juce::MathConstants<double>::pi * f);
                    mono += g * (readHist (0, now - d) + readHist (1, now - d)) * 0.5f;
                }
                const double pan = (v % 2 == 0 ? -1.0 : 1.0) * std::abs (frac) * width;   // alternate L/R
                const double a = (pan + 1.0) * 0.25 * juce::MathConstants<double>::pi;
                oL += mono * (float) std::cos (a); oR += mono * (float) std::sin (a);
            }
            const float norm = 1.0f / std::sqrt ((float) voices);
            outL = oL * norm + inL * S.p4; outR = oR * norm + inR * S.p4;
            break;
        }
        case Drift:
        {
            // Flume-style detune: pitch drifts gradually away from true over the window,
            // with an optional seasick wobble, beating against the dry signal underneath.
            const double semis = (S.p1 * 2.0 - 1.0) * 4.0;               // -4 .. +4 semitones total drift
            const double curve = 0.5 + 2.5 * S.p2;                        // early vs late bend
            const double prog = std::pow (t / len, curve);
            double r = std::pow (2.0, semis * prog / 12.0);
            const double wobCents = S.p3 * 50.0;                          // up to +/-50 cents of warble
            r *= std::pow (2.0, wobCents * std::sin (juce::MathConstants<double>::twoPi * 3.5 * t / sr) * prog / 1200.0);
            // time-varying pitch shift via dual crossfaded delay heads (handles up and down drift)
            const double W = 0.05 * sr;
            if (L.t == 0) S.readPos = 0.0;
            S.readPos += (1.0 - r) / W;                                   // phase accumulator
            const double now = (double) (L.eventStart + L.t);
            float shifted[2] = { 0, 0 };
            for (int k = 0; k < 2; ++k)
            {
                double f = S.readPos + k * 0.5; f -= std::floor (f);
                const double d = W * f;
                const float g = (float) std::sin (juce::MathConstants<double>::pi * f);
                shifted[0] += g * readHist (0, now - d);
                shifted[1] += g * readHist (1, now - d);
            }
            outL = shifted[0] + inL * S.p4;
            outR = shifted[1] + inR * S.p4;
            break;
        }
        case Sample:
        {
            const float g = S.p1 * 2.0f;
            outL = inL * S.p2; outR = inR * S.p2;
            const juce::SpinLock::ScopedTryLockType stl (sampleLock);
            const int n = stl.isLocked() ? L.sample.getNumSamples() : 0;
            if (n > 2)
            {
                const int startIdx = (int) (L.sStart * (n - 2));
                const int endIdx = juce::jlimit (startIdx + 2, n, (int) (L.sEnd * n));
                const int avail = endIdx - startIdx;
                double rate;
                if (L.sFit) rate = (double) avail / len;                       // sample ends exactly on the one
                else        rate = (L.sampleRateOfSample / sr) * std::pow (2.0, L.sPitch / 12.0);
                double sp = t * rate;
                if (sp < avail - 1)
                {
                    if (L.sRev) sp = (avail - 1) - sp;
                    sp += startIdx;
                    const int i0 = (int) sp; const float fr = (float) (sp - i0);
                    const int chR = juce::jmin (1, L.sample.getNumChannels() - 1);
                    const float* a = L.sample.getReadPointer (0); const float* b = L.sample.getReadPointer (chR);
                    outL += g * (a[i0] + (a[i0 + 1] - a[i0]) * fr);
                    outR += g * (b[i0] + (b[i0 + 1] - b[i0]) * fr);
                }
            }
            break;
        }
        case SpectralGate:
        {
            // streaming STFT gate: push input, pop overlap-added output (latency ~ one window)
            const int idx = S.sgIdx;
            S.sgFifo[0][(size_t) idx] = inL; S.sgFifo[1][(size_t) idx] = inR;
            outL = S.sgOut[0][(size_t) idx]; outR = S.sgOut[1][(size_t) idx];
            S.sgOut[0][(size_t) idx] = 0; S.sgOut[1][(size_t) idx] = 0;
            S.sgIdx = (idx + 1) % kFFTSize;
            if (++S.sgHopCount >= kHop) { S.sgHopCount = 0; spectralFrame (S); }
            break;
        }
        case RingMod:
        {
            const double freq = 20.0 * std::pow (2.0, S.p1 * 9.0);      // 20 Hz .. ~10 kHz
            const double ph = S.rmPhase;
            S.rmPhase = std::fmod (ph + freq / sr, 1.0);
            const float sine = (float) std::sin (juce::MathConstants<double>::twoPi * ph);
            const float square = sine >= 0 ? 1.0f : -1.0f;
            const float c = sine + (square - sine) * S.p2;              // carrier shape blend
            float wet[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float in = ch == 0 ? inL : inR;
                const float clean = in * c;
                // Parker's push-pull diode lattice
                const float u = 0.5f * in + c * 0.5f, w = 0.5f * in - c * 0.5f;
                const float diode = 2.0f * (diodeShape (u) + diodeShape (-u) - diodeShape (w) - diodeShape (-w));
                wet[ch] = clean + (diode - clean) * S.p3;
            }
            outL = wet[0] + inL * S.p4; outR = wet[1] + inR * S.p4;
            break;
        }
        case PingPong:
        {
            // alternate hard pans per synced step: L-R-L-R at 1/16, 1/32, whatever the rate says
            const double step = juce::jmax (32.0, spb / sliceDivFromP (S.p1));
            const int idx = (int) std::floor (t / step);
            const float target = ((idx & 1) == 0 ? -1.0f : 1.0f) * (0.4f + 0.6f * S.p2);
            const double slewMs = 0.2 + 14.0 * (1.0 - S.p3);            // slew high = instant hard cuts
            const double coef = std::exp (-1.0 / (slewMs * 0.001 * sr));
            if (L.t == 0) S.readPos = target;
            S.readPos = target + (S.readPos - target) * coef;
            const float pan = (float) S.readPos;
            const float mono = (inL + inR) * 0.5f;
            const float a = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            outL = mono * std::cos (a) * 1.41f + inL * S.p4;
            outR = mono * std::sin (a) * 1.41f + inR * S.p4;
            break;
        }
        case Orbit:
        {
            // 8D: smooth equal-power rotation around the head; behind = darker + slightly quieter
            const double rotPerBeat = 0.125 * std::pow (32.0, (double) S.p1);   // 1 turn per 8 beats .. 4 turns per beat
            const double az = juce::MathConstants<double>::twoPi * (t / spb) * rotPerBeat;
            const float pan = (float) std::sin (az) * (0.5f + 0.5f * S.p2);
            const float behind = (float) (0.5 - 0.5 * std::cos (az));
            if ((L.t & 15) == 0)
            {
                const float cut = 18000.0f * std::pow (0.06f, behind * S.p3);   // 18 kHz down to ~1 kHz behind you
                S.filter.setCutoffFrequency (juce::jlimit (400.0f, (float) sr * 0.45f, cut));
            }
            const float mono = S.filter.processSample (0, (inL + inR) * 0.5f);
            const float lvl = 1.0f - 0.3f * behind * S.p3;
            const float a = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            outL = mono * std::cos (a) * 1.41f * lvl + inL * S.p4;
            outR = mono * std::sin (a) * 1.41f * lvl + inR * S.p4;
            break;
        }
        case DelayThrow: case VerbThrow:
        {
            // a throw is a parallel send: pass the chain through untouched, feed the tail bus
            S.throwFeed[0] += inL; S.throwFeed[1] += inR;
            outL = inL; outR = inR;
            break;
        }
        default: break;
    }
    
}

// ---------------------------------------------------------------- process
void SofGlitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0) return;

    // ---- transport
    double bpm = (double) *pIntBpm;
    double ppqStart = 0.0;
    double beatsPerBar = 4.0;
    bool playing = false;
    bool haveHost = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto ts = pos->getTimeSignature()) beatsPerBar = ts->numerator * 4.0 / ts->denominator;
            if (auto p = pos->getPpqPosition()) { ppqStart = *p; haveHost = true; }
            playing = pos->getIsPlaying();
        }
    }
    const bool freeRun = *pFreeRun > 0.5f || wrapperType == wrapperType_Standalone;
    const bool run = (haveHost && playing) || freeRun;
    if (! (haveHost && playing))
    {
        ppqStart = internalPpq;
        if (freeRun) internalPpq += numSamples * bpm / (60.0 * sr);
    }
    else internalPpq = ppqStart;

    displayBpm = bpm; hostPlaying = run; displayPpq = ppqStart;
    const double ppqPerSample = bpm / (60.0 * sr);
    const double spb = 60.0 * sr / bpm;
    const double phaseBeats = (double) *pPhase * beatsPerBar;
    const uint32_t seed = (uint32_t) *pSeed;
    const float masterMix = *pMix;

    // ---- per-lane static params for this block
    struct LaneCfg { bool on; double unit; int group; int count; int every; float len; float prob; float mix; float gainLin; double cycleLen; double startBeat; int mode; int clen;
                     bool slotOn[kNumSlots]; int action[kNumSlots]; float p1[kNumSlots], p2[kNumSlots], p3[kNumSlots], p4[kNumSlots]; };
    LaneCfg cfg[kNumLanes];
    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& p = lp[(size_t) i];
        LaneCfg c;
        c.on = *p.enabled > 0.5f;
        c.unit = unitBeats ((int) *p.unit, beatsPerBar);
        c.group = juce::jmax (2, (int) *p.group);
        c.count = juce::jlimit (1, c.group - 1, (int) *p.count);
        c.every = juce::jmax (1, (int) *p.every);
        c.len = *p.length; c.prob = *p.prob; c.mix = *p.mix;
        c.gainLin = juce::Decibels::decibelsToGain ((float) *p.gain);
        for (int k = 0; k < kNumSlots; ++k)
        {
            const int aRaw = (int) *p.action[k];
            c.slotOn[k] = k == 0 || aRaw > 0;          // slot A is always on; B/C have an "(off)" entry at 0
            c.action[k] = k == 0 ? aRaw : aRaw - 1;
            c.p1[k] = *p.p1[k]; c.p2[k] = *p.p2[k]; c.p3[k] = *p.p3[k]; c.p4[k] = *p.p4[k];
        }
        c.cycleLen = c.unit * c.group;
        c.mode = (int) *p.mode;
        c.clen = juce::jmax (1, (int) *p.clen);
        c.startBeat = windowStartBeats ((int) *p.place, c.unit, c.group, c.count, *p.nudge);
        cfg[i] = c;
    }

    // resync lanes when playback starts or the playhead jumps, so we don't fire stale windows
    const bool jumped = ! wasRunning || std::abs (ppqStart - lastBlockEndPpq) > 0.25;
    if (run && jumped)
        for (int i = 0; i < kNumLanes; ++i)
        {
            lanes[(size_t) i].active = false;
            lanes[(size_t) i].lastCycle = (long) std::floor ((ppqStart - 1e-6 - phaseBeats - cfg[i].startBeat) / cfg[i].cycleLen);
        }
    wasRunning = run;
    lastBlockEndPpq = ppqStart + numSamples * ppqPerSample;

    // per-block prep for throw slots (delay time, reverb params)
    struct ThrowCfg { int lane, slot, action; int delaySamps; float feedback, damp, level; };
    ThrowCfg throws[kNumLanes * kNumSlots]; int numThrows = 0;
    static const double divs[] = { 0.125, 0.25, 0.375, 0.5, 0.75, 1.0, 1.5, 2.0 };   // beats: 1/32 .. 1/2
    for (int i = 0; i < kNumLanes; ++i)
        for (int k = 0; k < kNumSlots; ++k)
        {
            if (! cfg[i].on || ! cfg[i].slotOn[k]) continue;
            const int a = cfg[i].action[k];
            if (a != DelayThrow && a != VerbThrow) continue;
            auto& S = lanes[(size_t) i].slots[k];
            ThrowCfg tc { i, k, a, 0, 0, 0, cfg[i].p4[k] * 1.5f };
            if (a == DelayThrow)
            {
                const int di = juce::jlimit (0, 7, (int) (cfg[i].p1[k] * 7.999f));
                tc.delaySamps = juce::jlimit (16, (int) S.dline[0].size() - 2, (int) (divs[di] * spb));
                tc.feedback = 0.95f * cfg[i].p2[k];
                tc.damp = 0.99f * cfg[i].p3[k];
            }
            else
            {
                juce::Reverb::Parameters rp;
                rp.roomSize = cfg[i].p1[k]; rp.damping = cfg[i].p2[k]; rp.width = cfg[i].p3[k];
                rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
                S.reverb.setParameters (rp);
            }
            throws[numThrows++] = tc;
        }

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float* hL = history.getWritePointer (0);
    float* hR = history.getWritePointer (1);
    const double fadeLen = 0.002 * sr;
    bool anyActive[kNumLanes] = { false, false, false, false, false };

    for (int n = 0; n < numSamples; ++n)
    {
        const float inL = L[n];
        const float inR = R ? R[n] : inL;
        const int w = (int) (writePos % histSize);
        hL[w] = inL; hR[w] = inR;

        const double ppq = ppqStart + n * ppqPerSample - phaseBeats;

        // ---- evaluate lanes
        int winner = -1;
        for (int i = 0; i < kNumLanes; ++i)
        {
            auto& c = cfg[i];
            auto& S = lanes[(size_t) i];

            if (S.active)
            {
                if (S.t >= S.eventLen || ! run) S.active = false;
            }

            if (run && c.on && ! S.active)
            {
                // the window for cycle c starts at c*cycleLen + startBeat; fire the moment we cross it
                const long cycle = (long) std::floor ((ppq - c.startBeat) / c.cycleLen);
                if (cycle != S.lastCycle)
                {
                    S.lastCycle = cycle;
                    const bool rightCycle = ((cycle % c.every) + c.every) % c.every == c.every - 1;
                    const bool fires = c.mode == 0 ? (rightCycle && laneRoll (cycle, i, seed) < c.prob)
                                                  : getCustom (i, ((cycle % c.clen) + c.clen) % c.clen);
                    if (fires)
                    {
                        S.active = true;
                        S.eventStart = writePos;
                        S.eventLen = juce::jmax<int64_t> (64, (int64_t) (c.unit * c.count * c.len * spb));
                        S.t = 0;
                        S.mix = c.mix; S.gainLin = c.gainLin;
                        S.sPitch = *lp[(size_t) i].spitch; S.sRev = *lp[(size_t) i].srev > 0.5f;
                        S.sFit = *lp[(size_t) i].sfit > 0.5f; S.sStart = *lp[(size_t) i].sstart; S.sEnd = *lp[(size_t) i].send;
                        for (int k = 0; k < kNumSlots; ++k)
                        {
                            auto& sl = S.slots[k];
                            sl.on = c.slotOn[k]; sl.action = c.action[k];
                            sl.p1 = c.p1[k]; sl.p2 = c.p2[k]; sl.p3 = c.p3[k]; sl.p4 = c.p4[k];
                            sl.crushCount = 0; sl.readPos = (double) writePos;
                            sl.filter.reset();
                        }
                    }
                }
            }
            if (S.active && winner < 0) winner = i;
        }

        float outL = inL, outR = inR;
        if (winner >= 0)
        {
            auto& S = lanes[(size_t) winner];
            float wL = inL, wR = inR;
            for (auto& sl : S.slots)
                if (sl.on)
                {
                    float oL, oR;
                    renderSlot (S, sl, wL, wR, oL, oR);
                    wL = oL; wR = oR;
                }
            // edge fades + mixes
            const double tt = (double) S.t, rem = (double) (S.eventLen - S.t);
            float env = 1.0f;
            if (tt < fadeLen) env = (float) (tt / fadeLen);
            else if (rem < fadeLen) env = (float) (rem / fadeLen);
            const float m = env * S.mix * masterMix;
            outL = inL + (wL * S.gainLin - inL) * m;
            outR = inR + (wR * S.gainLin - inR) * m;
            anyActive[winner] = true;
        }
        for (int i = 0; i < kNumLanes; ++i) if (lanes[(size_t) i].active) lanes[(size_t) i].t++;

        // tail bus: throws keep ringing after their window closes
        for (int q = 0; q < numThrows; ++q)
        {
            auto& tc = throws[q];
            auto& S = lanes[(size_t) tc.lane].slots[tc.slot];
            float fl = S.throwFeed[0], fr = S.throwFeed[1];
            S.throwFeed[0] = 0; S.throwFeed[1] = 0;
            if (tc.action == DelayThrow)
            {
                const int size = (int) S.dline[0].size();
                const int rd = (S.dlWrite - tc.delaySamps + size) % size;
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float tap = S.dline[ch][(size_t) rd];
                    S.dampZ[ch] = tap + (S.dampZ[ch] - tap) * tc.damp;         // one-pole LP in the loop
                    S.dline[ch][(size_t) S.dlWrite] = (ch == 0 ? fl : fr) + S.dampZ[ch] * tc.feedback;
                    if (ch == 0) outL += tap * tc.level * masterMix; else outR += tap * tc.level * masterMix;
                }
                S.dlWrite = (S.dlWrite + 1) % size;
            }
            else
            {
                float wl = fl, wr = fr;
                S.reverb.processStereo (&wl, &wr, 1);
                outL += wl * tc.level * masterMix; outR += wr * tc.level * masterMix;
            }
        }

        L[n] = outL;
        if (R) R[n] = outR;
        ++writePos;
    }

    for (int i = 0; i < kNumLanes; ++i) laneActivity[i] = anyActive[i] ? 1.0f : 0.0f;
}

// ---------------------------------------------------------------- samples
bool SofGlitchProcessor::loadSampleForLane (int lane, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;
    const int n = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 20.0));
    juce::AudioBuffer<float> tmp ((int) juce::jmax (1u, reader->numChannels), n);
    reader->read (&tmp, 0, n, 0, true, true);
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        auto& S = lanes[(size_t) lane];
        S.sample = std::move (tmp);
        S.sampleRateOfSample = reader->sampleRate;
        S.samplePath = file.getFullPathName();
    }
    apvts.state.setProperty ("sample" + juce::String (lane), file.getFullPathName(), nullptr);
    return true;
}

juce::AudioBuffer<float> SofGlitchProcessor::getSampleCopy (int lane)
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    return lanes[(size_t) lane].sample;
}

void SofGlitchProcessor::setCustom (int lane, long cycle, bool on)
{
    if (cycle < 0 || cycle >= kCustomWords * 64) return;
    auto& w = customBits[(size_t) lane][(size_t) (cycle / 64)];
    const uint64_t bit = 1ULL << (cycle % 64);
    if (on) w |= bit; else w &= ~bit;
    storeCustomToState (lane);
}

void SofGlitchProcessor::fillCustomFromProbability (int lane)
{
    auto& p = lp[(size_t) lane];
    const int every = juce::jmax (1, (int) *p.every);
    const float prob = *p.prob;
    const uint32_t seed = (uint32_t) *pSeed;
    for (int w = 0; w < kCustomWords; ++w)
    {
        uint64_t word = 0;
        for (int b = 0; b < 64; ++b)
        {
            const long cycle = (long) w * 64 + b;
            const bool rightCycle = (cycle % every) == every - 1;
            if (rightCycle && laneRoll (cycle, lane, seed) < prob) word |= 1ULL << b;
        }
        customBits[(size_t) lane][(size_t) w] = word;
    }
    storeCustomToState (lane);
}

void SofGlitchProcessor::clearCustom (int lane)
{
    for (auto& w : customBits[(size_t) lane]) w = 0;
    storeCustomToState (lane);
}

int SofGlitchProcessor::duplicateLane (int src)
{
    int dest = -1;
    for (int i = 0; i < kNumLanes; ++i)
        if (i != src && *lp[(size_t) i].enabled < 0.5f) { dest = i; break; }
    if (dest < 0) return -1;

    static const char* names[] = { "unit", "group", "count", "every", "len", "prob", "mix", "gain", "place", "nudge", "mode",
                                   "clen", "send", "spitch", "srev", "sfit", "sstart",
                                   "action", "p1", "p2", "p3", "p4",
                                   "action_b", "p1_b", "p2_b", "p3_b", "p4_b",
                                   "action_c", "p1_c", "p2_c", "p3_c", "p4_c" };
    for (auto* n : names)
    {
        auto* from = apvts.getParameter (laneParam (src, n));
        auto* to = apvts.getParameter (laneParam (dest, n));
        if (from != nullptr && to != nullptr) to->setValueNotifyingHost (from->getValue());
    }
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        auto& a = lanes[(size_t) src]; auto& b = lanes[(size_t) dest];
        b.sample = a.sample; b.sampleRateOfSample = a.sampleRateOfSample; b.samplePath = a.samplePath;
    }
    apvts.state.setProperty ("sample" + juce::String (dest), lanes[(size_t) src].samplePath, nullptr);
    for (int w = 0; w < kCustomWords; ++w)
        customBits[(size_t) dest][(size_t) w] = customBits[(size_t) src][(size_t) w].load();
    storeCustomToState (dest);
    if (auto* on = apvts.getParameter (laneParam (dest, "on"))) on->setValueNotifyingHost (1.0f);
    return dest;
}

void SofGlitchProcessor::storeCustomToState (int lane)
{
    juce::String hex;
    for (auto& w : customBits[(size_t) lane]) hex << juce::String::toHexString ((juce::int64) w.load()) << ",";
    apvts.state.setProperty ("custom" + juce::String (lane), hex, nullptr);
}

void SofGlitchProcessor::loadCustomFromState (int lane)
{
    const auto hex = apvts.state.getProperty ("custom" + juce::String (lane)).toString();
    auto parts = juce::StringArray::fromTokens (hex, ",", "");
    for (int w = 0; w < kCustomWords; ++w)
        customBits[(size_t) lane][(size_t) w] = w < parts.size() ? (uint64_t) parts[w].getHexValue64() : 0ULL;
}

void SofGlitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SofGlitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    for (int i = 0; i < kNumLanes; ++i)
    {
        const auto path = apvts.state.getProperty ("sample" + juce::String (i)).toString();
        if (path.isNotEmpty()) loadSampleForLane (i, juce::File (path));
        loadCustomFromState (i);
    }
}

juce::AudioProcessorEditor* SofGlitchProcessor::createEditor() { return new SofGlitchEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SofGlitchProcessor(); }
