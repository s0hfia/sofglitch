#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace sg
{
constexpr int kNumLanes = 5;
constexpr int kNumSlots = 3;   // fx chain slots per lane

enum Action
{
    Cut = 0, Duck, Gate, Filter, Crush,
    Stutter, StutterRamp, Reverse, Hold, Pitch, OctaveUp, OctaveDown, PitchDive, TapeStop, Stretch, Drift, Unison,
    Sample,
    SpectralGate, RingMod, DelayThrow, VerbThrow,
    PingPong, Orbit,
    NumActions
};

inline const juce::StringArray& actionNames()
{
    static const juce::StringArray names {
        "Cut", "Duck", "Gate", "Filter", "Crush",
        "Stutter", "Stutter Ramp", "Reverse", "Hold", "Pitch", "Octave Up", "Octave Down", "Pitch Dive", "Tape Stop", "Stretch", "Detune", "Unison",
        "Sample",
        "Spectral Gate", "Ring Mod", "Delay Throw", "Verb Throw",
        "Ping Pong", "Orbit (8D)" };
    return names;
}

// What P1 / P2 mean for each action (shown in the UI)
inline juce::String p1Hint (int a)
{
    switch (a) {
        case Cut: return "-"; case Duck: return "depth"; case Gate: return "rate"; case Filter: return "from Hz";
        case Crush: return "bits"; case Stutter: return "slice"; case StutterRamp: return "slice"; case Reverse: return "-";
        case Hold: return "grain"; case Pitch: return "semis"; case OctaveUp: return "-"; case OctaveDown: return "-"; case PitchDive: return "dive"; case TapeStop: return "curve";
        case Stretch: return "slowdown"; case Drift: return "amount"; case Unison: return "cents";
        case Sample: return "gain";
        case SpectralGate: return "thresh"; case RingMod: return "freq"; case DelayThrow: return "time"; case VerbThrow: return "size";
        case PingPong: return "rate"; case Orbit: return "speed";
        default: return "P1"; }
}
inline juce::String p2Hint (int a)
{
    switch (a) {
        case Cut: return "-"; case Duck: return "-"; case Gate: return "duty"; case Filter: return "to Hz";
        case Crush: return "downsamp"; case Stutter: return "decay"; case StutterRamp: return "stages"; case Reverse: return "-";
        case Hold: return "-"; case Pitch: return "-"; case OctaveUp: return "-"; case OctaveDown: return "-"; case PitchDive: return "-"; case TapeStop: return "-";
        case Stretch: return "curve"; case Drift: return "curve"; case Unison: return "voices";
        case Sample: return "keep dry";
        case SpectralGate: return "reduce"; case RingMod: return "shape"; case DelayThrow: return "feedback"; case VerbThrow: return "damp";
        case PingPong: return "width"; case Orbit: return "width";
        default: return "P2"; }
}

// slot B/C dropdowns get an explicit off entry at index 0
inline const juce::StringArray& actionNamesWithOff()
{
    static juce::StringArray names = [] { juce::StringArray n; n.add ("(off)"); n.addArray (actionNames()); return n; }();
    return names;
}

inline const juce::StringArray& modeNames()
{
    static const juce::StringArray names { "Probability", "Custom" };
    return names;
}
constexpr int kCustomWords = 64;   // 64 x 64 bits = 4096 cycles per lane

inline const juce::StringArray& placeNames()
{
    static const juce::StringArray names { "Before the one", "On the one" };
    return names;
}
// start of the window, in beats, relative to the start of its cycle
inline double windowStartBeats (int place, double unit, int group, int count, float nudge)
{
    return (place == 0 ? (group - count) * unit : 0.0) + nudge * unit;
}

inline juce::String p3Hint (int a)
{
    switch (a) { case Stretch: return "grain"; case Drift: return "wobble"; case Unison: return "width";
               case SpectralGate: return "release"; case RingMod: return "diode"; case DelayThrow: return "damp"; case VerbThrow: return "width";
               case PingPong: return "slew"; case Orbit: return "darkness"; default: return ""; }
}
inline juce::String p4Hint (int a)
{
    switch (a) { case Stretch: return "pitch drop"; case Drift: return "dry"; case Unison: return "dry";
               case SpectralGate: return "invert"; case RingMod: return "dry"; case DelayThrow: return "level"; case VerbThrow: return "level";
               case PingPong: return "dry"; case Orbit: return "dry"; default: return ""; }
}

inline const juce::StringArray& unitNames()
{
    static const juce::StringArray names { "Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/8T", "1/16T" };
    return names;
}

inline double unitBeats (int idx, double beatsPerBar)
{
    switch (idx) {
        case 0: return beatsPerBar; case 1: return 2.0; case 2: return 1.0; case 3: return 0.5; case 4: return 0.25;
        case 5: return 0.125; case 6: return 0.0625; case 7: return 1.0 / 3.0; case 8: return 1.0 / 6.0;
        default: return 1.0; }
}

inline uint32_t hash32 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
}
// deterministic 0..1 roll for (cycle, lane, seed) -- shared by the engine and the timeline display
inline float laneRoll (long cycle, int lane, uint32_t seed)
{
    const uint32_t h = hash32 ((uint32_t) cycle * 2654435761U ^ (uint32_t) (lane + 1) * 40503U ^ seed * 97U);
    return (float) (h & 0xffffff) / (float) 0x1000000;
}

inline juce::String laneParam (int lane, const char* name) { return "l" + juce::String (lane) + "_" + name; }

constexpr int kFFTOrder = 10;
constexpr int kFFTSize = 1 << kFFTOrder;          // 1024
constexpr int kHop = kFFTSize / 4;                // 75% overlap (Hann, COLA sum = 1.5)

// One fx slot in a lane's chain
struct SlotState
{
    bool    on = false;
    int     action = 0;
    float   p1 = 0, p2 = 0, p3 = 0, p4 = 0;
    double  readPos = 0;      // for rate-varying readers
    float   crushHold[2] { 0, 0 };
    int     crushCount = 0;
    juce::dsp::StateVariableTPTFilter<float> filter;

    // ring mod
    double  rmPhase = 0;
    // spectral gate (STFT per-bin gate, Pd/SoundHack style)
    std::vector<float> sgFifo[2];      // kFFTSize circular input
    std::vector<float> sgOut[2];       // kFFTSize circular overlap-add output
    std::vector<float> sgGain;         // kFFTSize/2+1 smoothed per-bin gains
    int     sgIdx = 0, sgHopCount = 0;
    // delay throw (synced feedback delay, one-pole damping in the loop)
    std::vector<float> dline[2];
    int     dlWrite = 0;
    float   dampZ[2] { 0, 0 };
    // verb throw (Freeverb via juce::Reverb)
    juce::Reverb reverb;
    float   throwFeed[2] { 0, 0 };     // set by the chain during events, consumed by the tail bus
};

// Runtime state of one lane
struct LaneState
{
    bool    active = false;
    int64_t eventStart = 0;   // absolute history position where the event began
    int64_t eventLen = 0;
    int64_t t = 0;            // samples since event start
    long    lastCycle = -1;   // cycle index we last fired (or decided not to fire) on
    float   mix = 1, gainLin = 1;
    SlotState slots[kNumSlots];
    float   sPitch = 0, sStart = 0, sEnd = 1; bool sRev = false, sFit = false;
    juce::AudioBuffer<float> sample;   // per-lane sample slot
    double  sampleRateOfSample = 44100.0;
    juce::String samplePath;
};
}

class SofGlitchProcessor : public juce::AudioProcessor
{
public:
    SofGlitchProcessor();
    ~SofGlitchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // UI hooks
    bool loadSampleForLane (int lane, const juce::File& file);
    juce::AudioBuffer<float> getSampleCopy (int lane);
    bool getCustom (int lane, long cycle) const
    {
        if (cycle < 0 || cycle >= sg::kCustomWords * 64) return false;
        return (customBits[(size_t) lane][(size_t) (cycle / 64)].load() >> (cycle % 64)) & 1ULL;
    }
    void setCustom (int lane, long cycle, bool on);
    void fillCustomFromProbability (int lane);
    void clearCustom (int lane);
    int duplicateLane (int src);   // copy a lane onto the next free (disabled) lane; returns dest or -1
    juce::String getSamplePathForLane (int lane) const { return lanes[(size_t) lane].samplePath; }
    std::atomic<float> laneActivity[sg::kNumLanes];   // 0/1 for LEDs
    std::atomic<double> displayPpq { 0.0 };
    std::atomic<double> displayBpm { 120.0 };
    std::atomic<bool>   hostPlaying { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // history ring buffer
    juce::AudioBuffer<float> history;
    int64_t writePos = 0;   // absolute sample counter
    int     histSize = 0;
    double  sr = 44100.0;
    double  internalPpq = 0.0;
    double  lastBlockEndPpq = -1e9;
    bool    wasRunning = false;

    std::array<sg::LaneState, sg::kNumLanes> lanes;
    std::array<std::array<std::atomic<uint64_t>, sg::kCustomWords>, sg::kNumLanes> customBits;
    void storeCustomToState (int lane);
    void loadCustomFromState (int lane);
    juce::AudioFormatManager formatManager;
    juce::SpinLock sampleLock;

    inline float readHist (int ch, double absPos) const;
    void renderSlot (sg::LaneState& L, sg::SlotState& S, float inL, float inR, float& outL, float& outR);
    void spectralFrame (sg::SlotState& S);
    juce::dsp::FFT fft { sg::kFFTOrder };
    std::vector<float> hannWin;                    // kFFTSize
    float fftWork[2][sg::kFFTSize * 2];           // shared scratch (audio thread only)

    // cached per-lane params (atomic pointers)
    struct LaneParams
    {
        std::atomic<float>* enabled; std::atomic<float>* unit; std::atomic<float>* group; std::atomic<float>* count;
        std::atomic<float>* every; std::atomic<float>* length; std::atomic<float>* prob; std::atomic<float>* mix; std::atomic<float>* gain;
        std::atomic<float>* place; std::atomic<float>* nudge; std::atomic<float>* mode; std::atomic<float>* clen; std::atomic<float>* send;
        std::atomic<float>* spitch; std::atomic<float>* srev; std::atomic<float>* sfit; std::atomic<float>* sstart;
        std::atomic<float>* action[sg::kNumSlots];
        std::atomic<float>* p1[sg::kNumSlots]; std::atomic<float>* p2[sg::kNumSlots];
        std::atomic<float>* p3[sg::kNumSlots]; std::atomic<float>* p4[sg::kNumSlots];
    };
    std::array<LaneParams, sg::kNumLanes> lp;
    std::atomic<float>* pMix; std::atomic<float>* pSeed; std::atomic<float>* pPhase; std::atomic<float>* pFreeRun; std::atomic<float>* pIntBpm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SofGlitchProcessor)
};
