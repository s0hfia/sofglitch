#pragma once
#include "PluginProcessor.h"

// Everskies-style look & feel: pastel pixel-art chrome, Silkscreen pixel font
class SgLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SgLookAndFeel();
    void applyTheme();
    juce::Font pixel (float h, bool bold = false) const;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return pixel (11.0f); }
    juce::Font getPopupMenuFont() override { return pixel (12.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return pixel (10.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
private:
    juce::Typeface::Ptr reg, bold;
};

// pixel disclosure triangle for the track stack
class DiscloseButton : public juce::Button
{
public:
    DiscloseButton() : juce::Button ("disclose") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool, bool) override;
};

// waveform with draggable start/end selection, bound to a lane's sstart/send params
class SampleView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    SampleView (SofGlitchProcessor& p, int lane);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    void timerCallback() override;
    SofGlitchProcessor& proc; int lane;
    juce::String lastPath; std::vector<float> mins, maxs; bool empty = true;
    float dragAnchor = 0; juce::RangedAudioParameter* pStart; juce::RangedAudioParameter* pEnd;
};

class SofGlitchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SofGlitchEditor (SofGlitchProcessor&);
    ~SofGlitchEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider slider; juce::Label label; std::unique_ptr<SA> att; };

    struct LaneUI
    {
        juce::ToggleButton on;   std::unique_ptr<BA> onAtt;
        juce::TextButton dup { "dup" };
        DiscloseButton disclose;
        juce::ComboBox action;   std::unique_ptr<CA> actionAtt;      // slot A, in the track header
        juce::ComboBox actionB, actionC; std::unique_ptr<CA> actionBAtt, actionCAtt;  // sub-rows
        juce::ComboBox unit;     std::unique_ptr<CA> unitAtt;
        juce::ComboBox place;    std::unique_ptr<CA> placeAtt;
        juce::ComboBox mode;     std::unique_ptr<CA> modeAtt;
        juce::TextButton fill { "fill from prob" }, clear { "clear" };
        Knob nudge, group, count, every, len, prob, mix, gain, clen;
        Knob p1, p2, p3, p4, p1b, p2b, p3b, p4b, p1c, p2c, p3c, p4c;
        juce::TextButton load { "load sample..." };
        juce::Label sampleName;
        Knob sPitch, sStart, sEnd;
        juce::ToggleButton sRev { "reverse" }, sFit { "fit to window" };
        std::unique_ptr<BA> sRevAtt, sFitAtt;
        std::unique_ptr<SampleView> wave;
        std::vector<juce::Component*> inspectorComps, sampleComps;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& text, const juce::String& tip, bool integer);
    void updateHints (int lane);
    void selectLane (int lane);
    void layoutTracks();
    void layoutInspector (int lane);
    void drawBackground (juce::Graphics&);
    void drawTimeline (juce::Graphics&);
    int  visibleBars() const;
    int  laneRowTop (int lane) const;      // y offset of a lane's block inside the timeline (below ruler)
    int  laneRowHeight (int lane) const;   // main row + any expanded sub-rows
    juce::Rectangle<int> laneMainRow (int lane) const;
    juce::Rectangle<int> laneFullRow (int lane) const;

    SofGlitchProcessor& proc;
    SgLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 400 };
    std::array<LaneUI, sg::kNumLanes> lanes;
    bool expanded[sg::kNumLanes] { false, false, false, false, false };
    int selected = 0;
    int viewStartBar = 0;
    float wheelAccum = 0;
    juce::uint32 lastManualScroll = 0;

    Knob masterMix, seed, phase, intBpm;
    juce::ToggleButton freeRun { "free run" }; std::unique_ptr<BA> freeRunAtt;
    juce::ComboBox zoom;
    juce::ToggleButton darkToggle { "night" };
    void applyWidgetTheme();
    juce::Label title, transport, inspectorTitle;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> timelineArea, inspectorArea;
    static constexpr int kHeaderW = 250, kRulerH = 24, kRowH = 52, kSubH = 26;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SofGlitchEditor)
};
