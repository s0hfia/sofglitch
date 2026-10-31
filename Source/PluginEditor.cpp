#include "PluginEditor.h"
#include "BinaryData.h"

using namespace sg;

// ---------------------------------------------------------------- theme (everskies day / night)
struct Theme
{
    juce::Colour skyTop, skyMid, skyBot, ivory, ivoryDim, plum, plumSoft, chkA, chkB, bevelHi;
    float cloudAlpha, sparkAlpha;
};
static const Theme kDay {
    juce::Colour (0xffc9d4ff), juce::Colour (0xfff3d9f2), juce::Colour (0xffcfe9ff),
    juce::Colour (0xfffffaf4), juce::Colour (0xfff4ecf7),
    juce::Colour (0xff6b5a80), juce::Colour (0xff9a8ab0),
    juce::Colour (0xffe7d9fb), juce::Colour (0xfffdeaf6),
    juce::Colour (0xccffffff), 0.85f, 0.45f };
static const Theme kNight {
    juce::Colour (0xff251f3a), juce::Colour (0xff3a2d55), juce::Colour (0xff1c2740),
    juce::Colour (0xff2e2940), juce::Colour (0xff262133),
    juce::Colour (0xffe8def7), juce::Colour (0xffa89ac4),
    juce::Colour (0xff443a62), juce::Colour (0xff372f52),
    juce::Colour (0x1fffffff), 0.20f, 0.85f };
static Theme TH = kDay;
static const juce::Colour kPinkA (0xffff9ed2);   // accents shared by both themes
static const juce::Colour kGold  (0xffffd98a);

static juce::Colour actionColour (int a)
{
    if (a == Sample) return juce::Colour (0xffefb73f);                       // gold: populate
    if (a == DelayThrow || a == VerbThrow) return juce::Colour (0xff56c98b); // green: throws
    if (a == PingPong || a == Orbit) return juce::Colour (0xffa678f2);       // purple: spatial
    if (a >= Stutter && a <= Unison) return juce::Colour (0xfff2699c);       // pink: buffer mangling
    return juce::Colour (0xff5fb2e8);                                        // blue: processors
}

static juce::String actionTip (int a)
{
    switch (a) {
        case Cut: return "Silence the window. The classic breath before the one.";
        case Duck: return "Drop the volume for the window. P1 = how far down.";
        case Gate: return "Chop the window with a tremolo. P1 = rate (1/8 .. 1/256), P2 = duty cycle.";
        case Filter: return "Sweep a resonant low-pass across the window. P1 = start Hz, P2 = end Hz.";
        case Crush: return "Bitcrush + downsample. P1 = bit depth (16 -> 2), P2 = sample-rate reduction.";
        case Stutter: return "Capture the first slice of the window and retrigger it. P1 = slice (1/8 .. 1/256), P2 = decay per repeat.";
        case StutterRamp: return "Stutter whose slice halves in stages toward the one - the classic build. P1 = starting slice, P2 = number of stages.";
        case Reverse: return "Play the audio just before the window backwards into the one.";
        case Hold: return "Freeze a micro-grain at the window start into a drone. P1 = grain size (2-100 ms).";
        case Pitch: return "Repitch the window. P1 = -24 .. +24 semitones (0.5 = unity).";
        case OctaveUp: return "Play the window an octave up.";
        case OctaveDown: return "Play the window an octave down.";
        case PitchDive: return "Pitch falls across the window. P1 = how far it dives.";
        case TapeStop: return "Tape-stop across the window. P1 = curve sharpness.";
        case Stretch: return "Time-stretch transition: the audio slows down (granular, pitch stays put) into the one. P1 = how much it slows (1 = freeze), P2 = curve, P3 = grain size, P4 = blend toward tape-stop pitch drop.";
        case Drift: return "Flume-style detune: the pitch slowly bends away from true over the window. P1 = how far and which way (0.5 = none), P2 = curve, P3 = seasick wobble, P4 = dry under - turn this up to hear the beating.";
        case Unison: return "Unison detune: stacks pitch-shifted copies of the window. P1 = spread in cents, P2 = voices (2-8), P3 = stereo width, P4 = dry under.";
        case Sample: return "Play this lane's sample in the window. P1 = gain, P2 = how much dry stays under it.";
        case SpectralGate: return "STFT per-bin gate: only loud frequencies pass. P1 = threshold, P2 = cut depth, P3 = release, P4 = INVERT - keep only the quiet residue. Smears ~20 ms; that smear is the sound.";
        case RingMod: return "Ring modulator with Parker's diode-lattice model (as in Surge XT). P1 = carrier freq, P2 = sine->square, P3 = diode grit, P4 = dry under.";
        case DelayThrow: return "Dub delay throw: the window is sent into a synced feedback delay that rings past the one. P1 = time, P2 = feedback, P3 = damping, P4 = throw level.";
        case VerbThrow: return "Reverb throw (Freeverb): the window is flung into a reverb tail that rings past the one. P1 = size, P2 = damping, P3 = width, P4 = throw level.";
        case PingPong: return "Alternate hard pans per step: left-right-left-right at the chosen rate. P1 = step (1/8 .. 1/256), P2 = hardness, P3 = slew (high = instant cuts), P4 = dry under.";
        case Orbit: return "8D audio: the sound circles your head. P1 = orbit speed, P2 = width, P3 = darkness behind the head, P4 = dry under.";
        default: return {}; }
}

// ---------------------------------------------------------------- pixel helpers
static void pxRect (juce::Graphics& g, float x, float y, float s) { g.fillRect (x, y, s, s); }

// draw a tiny bitmap ('X' = filled) on a pixel grid
static void pxBitmap (juce::Graphics& g, const char* const* rows, int nRows, float x, float y, float s, juce::Colour c)
{
    g.setColour (c);
    for (int r = 0; r < nRows; ++r)
        for (int col = 0; rows[r][col] != 0; ++col)
            if (rows[r][col] == 'X') pxRect (g, x + col * s, y + r * s, s);
}

static const char* kHeart[] = { ".X.X.", "XXXXX", "XXXXX", ".XXX.", "..X.." };
static const char* kSparkle[] = { "..X..", "..X..", "XXXXX", "..X..", "..X.." };
static const char* kCloud[] = {
    ".....XXXXX..........",
    "...XXXXXXXXX..XXXX..",
    "..XXXXXXXXXXXXXXXXX.",
    ".XXXXXXXXXXXXXXXXXXX",
    "XXXXXXXXXXXXXXXXXXXX",
    ".XXXXXXXXXXXXXXXXXX."
};

// pastel bevel card: ivory body, plum outline, soft top highlight
static void bevelCard (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour body, float corner = 4.0f)
{
    g.setColour (body); g.fillRoundedRectangle (r, corner);
    g.setColour (TH.bevelHi);
    g.drawLine (r.getX() + corner, r.getY() + 1.5f, r.getRight() - corner, r.getY() + 1.5f, 1.5f);
    g.setColour (TH.plum); g.drawRoundedRectangle (r.reduced (0.75f), corner, 1.5f);
}

// ---------------------------------------------------------------- look & feel
SgLookAndFeel::SgLookAndFeel()
{
    reg = juce::Typeface::createSystemTypefaceFor (BinaryData::SilkscreenRegular_ttf, BinaryData::SilkscreenRegular_ttfSize);
    bold = juce::Typeface::createSystemTypefaceFor (BinaryData::SilkscreenBold_ttf, BinaryData::SilkscreenBold_ttfSize);
    applyTheme();
}

void SgLookAndFeel::applyTheme()
{
    setColour (juce::ComboBox::backgroundColourId, TH.ivory);
    setColour (juce::ComboBox::outlineColourId, TH.plum);
    setColour (juce::ComboBox::textColourId, TH.plum);
    setColour (juce::ComboBox::arrowColourId, TH.plum);
    setColour (juce::PopupMenu::backgroundColourId, TH.ivory);
    setColour (juce::PopupMenu::textColourId, TH.plum);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kPinkA);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, TH.plum);
    setColour (juce::TextButton::textColourOffId, TH.plum);
    setColour (juce::Slider::textBoxTextColourId, TH.plum);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, TH.ivory);
    setColour (juce::TooltipWindow::textColourId, TH.plum);
    setColour (juce::TooltipWindow::outlineColourId, TH.plum);
    setColour (juce::Label::textColourId, TH.plum);
    // the slider textbox turns into a TextEditor while typing - make it readable
    setColour (juce::TextEditor::textColourId, TH.plum);
    setColour (juce::TextEditor::highlightedTextColourId, TH.plum);
    setColour (juce::TextEditor::backgroundColourId, TH.ivory);
    setColour (juce::TextEditor::highlightColourId, kPinkA.withAlpha (0.5f));
    setColour (juce::TextEditor::focusedOutlineColourId, kPinkA);
    setColour (juce::TextEditor::outlineColourId, TH.plumSoft);
    setColour (juce::CaretComponent::caretColourId, TH.plum);
    setColour (juce::Label::textWhenEditingColourId, TH.plum);
}

juce::Font SgLookAndFeel::pixel (float h, bool b) const
{
    return juce::Font (juce::FontOptions ((b && bold != nullptr) ? bold : reg).withHeight (h));
}

void SgLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f - 2.0f;
    const auto c = r.getCentre();
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float vAngle = start + pos * (end - start);
    constexpr int nSeg = 13;                     // chunky pixel arc
    const float blk = juce::jmax (3.0f, radius * 0.30f);
    for (int i = 0; i < nSeg; ++i)
    {
        const float a = start + (end - start) * (float) i / (nSeg - 1);
        const auto pt = c.getPointOnCircumference (radius, a);
        const bool lit = a <= vAngle + 0.001f;
        g.setColour (lit ? accent : TH.ivoryDim.darker (0.06f));
        g.fillRect (pt.x - blk * 0.5f, pt.y - blk * 0.5f, blk, blk);
        g.setColour (TH.plum.withAlpha (lit ? 0.9f : 0.35f));
        g.drawRect (pt.x - blk * 0.5f, pt.y - blk * 0.5f, blk, blk, 1.0f);
    }
    const auto tip = c.getPointOnCircumference (radius * 0.45f, vAngle);
    g.setColour (TH.plum);
    g.fillRect (tip.x - 2.0f, tip.y - 2.0f, 4.0f, 4.0f);
}

void SgLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool isDown, int, int, int, int, juce::ComboBox& cb)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    bevelCard (g, r, isDown ? TH.ivoryDim : cb.findColour (juce::ComboBox::backgroundColourId), 3.0f);
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f - 2.0f;
    g.setColour (TH.plum);
    g.fillRect (ax, ay, 7.0f, 2.0f); g.fillRect (ax + 1.5f, ay + 2.0f, 4.0f, 2.0f); g.fillRect (ax + 2.5f, ay + 4.0f, 2.0f, 2.0f);
}

void SgLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const float s = 13.0f;
    auto box = juce::Rectangle<float> (2.0f, b.getHeight() * 0.5f - s * 0.5f, s, s);
    bevelCard (g, box, b.getToggleState() ? kPinkA : TH.ivory, 2.0f);
    if (b.getToggleState())
    {
        g.setColour (juce::Colours::white);
        g.fillRect (box.getX() + 3.0f, box.getY() + 6.0f, 2.5f, 2.5f);
        g.fillRect (box.getX() + 5.5f, box.getY() + 8.0f, 2.5f, 2.5f);
        g.fillRect (box.getX() + 8.0f, box.getY() + 3.5f, 2.5f, 4.0f);
    }
    g.setColour (TH.plum);
    g.setFont (pixel (10.0f));
    g.drawText (b.getButtonText(), (int) (s + 6), 0, b.getWidth() - (int) s - 6, b.getHeight(), juce::Justification::centredLeft);
}

void SgLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    bevelCard (g, r, down ? TH.chkA : over ? TH.ivoryDim : TH.ivory, 3.0f);
}

void DiscloseButton::paintButton (juce::Graphics& g, bool, bool)
{
    g.setColour (TH.plum);
    const float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f;
    if (getToggleState())
    {   // down triangle, pixel steps
        g.fillRect (cx - 5.0f, cy - 3.0f, 10.0f, 2.5f);
        g.fillRect (cx - 3.0f, cy - 0.5f, 6.0f, 2.5f);
        g.fillRect (cx - 1.0f, cy + 2.0f, 2.0f, 2.5f);
    }
    else
    {   // right triangle
        g.fillRect (cx - 3.0f, cy - 5.0f, 2.5f, 10.0f);
        g.fillRect (cx - 0.5f, cy - 3.0f, 2.5f, 6.0f);
        g.fillRect (cx + 2.0f, cy - 1.0f, 2.5f, 2.0f);
    }
}

// ---------------------------------------------------------------- sample view
SampleView::SampleView (SofGlitchProcessor& p, int l) : proc (p), lane (l)
{
    pStart = proc.apvts.getParameter (laneParam (lane, "sstart"));
    pEnd = proc.apvts.getParameter (laneParam (lane, "send"));
    setTooltip ("Drag to select the part of the sample that plays. Fine-tune with START / END.");
    refresh();
    startTimerHz (10);
}

void SampleView::timerCallback()
{
    if (proc.getSamplePathForLane (lane) != lastPath) refresh();
    repaint();
}

void SampleView::refresh()
{
    lastPath = proc.getSamplePathForLane (lane);
    auto buf = proc.getSampleCopy (lane);
    const int n = buf.getNumSamples();
    const int px = 400;
    mins.assign ((size_t) px, 0.0f); maxs.assign ((size_t) px, 0.0f);
    empty = n < 2;
    if (empty) return;
    for (int x = 0; x < px; ++x)
    {
        const int a = (int) ((juce::int64) x * n / px), b = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * n / px));
        float lo = 0, hi = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto mm = buf.findMinMax (ch, a, b - a);
            lo = juce::jmin (lo, mm.getStart()); hi = juce::jmax (hi, mm.getEnd());
        }
        mins[(size_t) x] = lo; maxs[(size_t) x] = hi;
    }
}

void SampleView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    bevelCard (g, r, TH.ivoryDim, 3.0f);
    if (empty)
    {
        g.setColour (TH.plumSoft);
        if (auto* lf = dynamic_cast<SgLookAndFeel*> (&getLookAndFeel())) g.setFont (lf->pixel (10.0f));
        g.drawText ("no sample loaded", getLocalBounds(), juce::Justification::centred);
        return;
    }
    auto inner = r.reduced (3.0f);
    const float s0 = pStart->getValue(), s1 = pEnd->getValue();
    const float x0 = inner.getX() + s0 * inner.getWidth(), x1 = inner.getX() + juce::jmax (s0, s1) * inner.getWidth();
    g.setColour (kGold.withAlpha (0.35f)); g.fillRect (x0, inner.getY(), x1 - x0, inner.getHeight());
    const float mid = inner.getCentreY(), half = inner.getHeight() * 0.46f;
    const int px = (int) mins.size();
    for (int x = 0; x < (int) inner.getWidth(); x += 2)   // chunky 2px columns for the pixel look
    {
        const int i = juce::jlimit (0, px - 1, (int) ((float) x / inner.getWidth() * px));
        const float fx = inner.getX() + (float) x;
        const bool inSel = fx >= x0 && fx <= x1;
        g.setColour (inSel ? juce::Colour (0xffefb73f) : TH.plumSoft.withAlpha (0.65f));
        g.fillRect (fx, mid - maxs[(size_t) i] * half, 2.0f, juce::jmax (1.0f, (maxs[(size_t) i] - mins[(size_t) i]) * half));
    }
    g.setColour (juce::Colour (0xffefb73f));
    g.fillRect (x0 - 1.0f, inner.getY(), 2.0f, inner.getHeight());
    g.fillRect (x1 - 1.0f, inner.getY(), 2.0f, inner.getHeight());
}

void SampleView::mouseDown (const juce::MouseEvent& e)
{
    dragAnchor = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth());
    pStart->beginChangeGesture(); pEnd->beginChangeGesture();
    pStart->setValueNotifyingHost (dragAnchor); pEnd->setValueNotifyingHost (juce::jmin (1.0f, dragAnchor + 0.01f));
}
void SampleView::mouseDrag (const juce::MouseEvent& e)
{
    const float x = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth());
    pStart->setValueNotifyingHost (juce::jmin (dragAnchor, x)); pEnd->setValueNotifyingHost (juce::jmax (dragAnchor, x));
    repaint();
}
void SampleView::mouseUp (const juce::MouseEvent&) { pStart->endChangeGesture(); pEnd->endChangeGesture(); }

// ---------------------------------------------------------------- editor
void SofGlitchEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& text, const juce::String& tip, bool integer)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 46, 13);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, kPinkA);
    k.slider.setNumDecimalPlacesToDisplay (integer ? 0 : 2);
    k.slider.setTooltip (tip);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, TH.plumSoft);
    k.label.setFont (lnf.pixel (9.0f));
    k.label.setTooltip (tip);
    addChildComponent (k.slider);
    addChildComponent (k.label);
    k.att = std::make_unique<SA> (proc.apvts, paramId, k.slider);
}

SofGlitchEditor::SofGlitchEditor (SofGlitchProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    title.setText ("SOFGLITCH", juce::dontSendNotification);
    title.setFont (lnf.pixel (24.0f, true));
    title.setColour (juce::Label::textColourId, TH.plum);
    addAndMakeVisible (title);
    transport.setColour (juce::Label::textColourId, TH.plumSoft);
    transport.setFont (lnf.pixel (11.0f));
    transport.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (transport);
    inspectorTitle.setFont (lnf.pixel (11.0f, true));
    inspectorTitle.setColour (juce::Label::textColourId, TH.plum);
    addAndMakeVisible (inspectorTitle);

    setupKnob (masterMix, "mix", "MIX", "Global wet/dry. 1 = full effect.", false);
    setupKnob (seed, "seed", "SEED", "Random seed. Deterministic - same seed, same glitches every playback. Change it to re-roll.", true);
    setupKnob (phase, "phase", "PHRASE", "Shift the grouping by N bars, for songs whose 4-bar phrases don't start on bar 1.", true);
    setupKnob (intBpm, "intbpm", "BPM", "Tempo used when there is no host transport (Free Run / standalone).", true);
    for (auto* k : { &masterMix, &seed, &phase, &intBpm }) { k->slider.setVisible (true); k->label.setVisible (true); }
    freeRun.setTooltip ("Keep the internal clock running even when the host transport is stopped.");
    addAndMakeVisible (freeRun);
    freeRunAtt = std::make_unique<BA> (proc.apvts, "freerun", freeRun);
    zoom.addItemList ({ "4 bars", "8 bars", "16 bars", "32 bars" }, 1);
    zoom.setSelectedId (2, juce::dontSendNotification);
    zoom.setTooltip ("How many bars the timeline shows. Scroll with the mouse wheel; it follows the playhead while playing.");
    zoom.onChange = [this] { repaint(); };
    addAndMakeVisible (zoom);

    darkToggle.setTooltip ("Everskies night mode: midnight sky, moonlit clouds, brighter sparkles.");
    addAndMakeVisible (darkToggle);
    darkToggle.setToggleState ((bool) proc.apvts.state.getProperty ("darkui", false), juce::dontSendNotification);
    darkToggle.onClick = [this]
    {
        proc.apvts.state.setProperty ("darkui", darkToggle.getToggleState(), nullptr);
        TH = darkToggle.getToggleState() ? kNight : kDay;
        lnf.applyTheme();
        applyWidgetTheme();
        repaint();
    };
    TH = darkToggle.getToggleState() ? kNight : kDay;
    lnf.applyTheme();

    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& U = lanes[(size_t) i];
        auto id = [i] (const char* n) { return laneParam (i, n); };

        addAndMakeVisible (U.disclose);
        U.disclose.setTooltip ("Expand this level's fx stack (like a Logic track stack).");
        U.disclose.onClick = [this, i] { expanded[i] = lanes[(size_t) i].disclose.getToggleState(); layoutTracks(); repaint(); };

        U.on.setButtonText ("L" + juce::String (i + 1));
        U.on.setTooltip ("Enable this level.");
        addAndMakeVisible (U.on);
        U.onAtt = std::make_unique<BA> (proc.apvts, id ("on"), U.on);

        U.dup.setTooltip ("Copy this whole level (timing, fx chain, sample, custom pattern) onto the next free lane.");
        addAndMakeVisible (U.dup);
        U.dup.onClick = [this, i]
        {
            const int dest = proc.duplicateLane (i);
            if (dest >= 0) { lanes[(size_t) dest].sampleName.setText (juce::File (proc.getSamplePathForLane (dest)).getFileName(), juce::dontSendNotification); selectLane (dest); }
        };

        U.action.addItemList (actionNames(), 1);
        addAndMakeVisible (U.action);
        U.actionAtt = std::make_unique<CA> (proc.apvts, id ("action"), U.action);
        U.action.onChange = [this, i] { updateHints (i); layoutInspector (selected); repaint(); };

        for (auto* cb : { &U.actionB, &U.actionC })
        {
            cb->addItemList (actionNamesWithOff(), 1);
            cb->setTooltip ("Add another effect to this level's chain. Order: A then B then C. Transformers (Cut/Duck/Gate/Filter/Crush/Ring Mod/Spectral Gate/pans) process what came before; the rest resynthesize and treat it as their dry.");
            cb->onChange = [this, i] { updateHints (i); layoutInspector (selected); layoutTracks(); repaint(); };
            addChildComponent (*cb);
        }
        U.actionBAtt = std::make_unique<CA> (proc.apvts, id ("action_b"), U.actionB);
        U.actionCAtt = std::make_unique<CA> (proc.apvts, id ("action_c"), U.actionC);

        U.unit.addItemList (unitNames(), 1);
        U.unit.setTooltip ("The grid unit this level counts in.");
        U.unit.onChange = [this] { repaint(); };
        addChildComponent (U.unit);
        U.unitAtt = std::make_unique<CA> (proc.apvts, id ("unit"), U.unit);

        U.place.addItemList (placeNames(), 1);
        U.place.setTooltip ("BEFORE THE ONE: the window ends on the downbeat (anticipation). ON THE ONE: the window starts on the downbeat and swallows it.");
        U.place.onChange = [this] { layoutInspector (selected); repaint(); };
        addChildComponent (U.place);
        U.placeAtt = std::make_unique<CA> (proc.apvts, id ("place"), U.place);

        U.mode.addItemList (modeNames(), 1);
        U.mode.setTooltip ("PROBABILITY: windows fire by the seeded dice roll. CUSTOM: click clips in the timeline to toggle them.");
        U.mode.onChange = [this] { layoutInspector (selected); repaint(); };
        addChildComponent (U.mode);
        U.modeAtt = std::make_unique<CA> (proc.apvts, id ("mode"), U.mode);

        U.fill.setTooltip ("Copy the current probability pattern into the custom pattern.");
        U.fill.onClick = [this, i] { proc.fillCustomFromProbability (i); repaint(); };
        U.clear.setTooltip ("Turn every custom clip off.");
        U.clear.onClick = [this, i] { proc.clearCustom (i); repaint(); };
        addChildComponent (U.fill); addChildComponent (U.clear);

        setupKnob (U.nudge, id ("nudge"), "NUDGE", "Shift the window by this many grid units (-32 .. +32). 0 = exactly on the chosen placement.", false);
        setupKnob (U.group, id ("group"), "GROUP", "How many units make one cycle. 1/4 x 16 = a 4-bar phrase. Odd numbers (7, 5) give the 'ends on 7' feel.", true);
        setupKnob (U.count, id ("count"), "WIDTH", "How many grid units wide the window is. WIDTH 2 at 1/16 = the two 16ths before (or on) the one.", true);
        setupKnob (U.every, id ("every"), "EVERY", "Only fire on every Nth cycle (the last of each N).", true);
        setupKnob (U.len, id ("len"), "LENGTH", "How much of the window the action lasts. 1 = right up to the one.", false);
        setupKnob (U.prob, id ("prob"), "PROB", "Chance each window fires. Seeded and deterministic. Ghost clips are the ones that won't fire.", false);
        setupKnob (U.mix, id ("mix"), "MIX", "Wet/dry for this level only.", false);
        setupKnob (U.clen, id ("clen"), "PATTERN", "Custom-mode pattern length, in cycles. The pattern you click in loops every N cycles - 16 with a 1-bar cycle = a 16-bar pattern.", true);
        U.clen.slider.onValueChange = [this] { repaint(); };
        setupKnob (U.gain, id ("gain"), "GAIN", "Boost (or trim) the effected window, in dB, before the mix. Panned/glitched material often wants +3 to +9.", false);
        setupKnob (U.p1, id ("p1"), "P1", "", false);
        setupKnob (U.p2, id ("p2"), "P2", "", false);
        setupKnob (U.p3, id ("p3"), "P3", "", false);
        setupKnob (U.p4, id ("p4"), "P4", "", false);
        setupKnob (U.p1b, id ("p1_b"), "P1", "", false);
        setupKnob (U.p2b, id ("p2_b"), "P2", "", false);
        setupKnob (U.p3b, id ("p3_b"), "P3", "", false);
        setupKnob (U.p4b, id ("p4_b"), "P4", "", false);
        setupKnob (U.p1c, id ("p1_c"), "P1", "", false);
        setupKnob (U.p2c, id ("p2_c"), "P2", "", false);
        setupKnob (U.p3c, id ("p3_c"), "P3", "", false);
        setupKnob (U.p4c, id ("p4_c"), "P4", "", false);
        for (auto* k : { &U.group, &U.count, &U.every, &U.len, &U.prob, &U.nudge })
            k->slider.onValueChange = [this] { repaint(); };

        U.load.setTooltip ("Load a WAV/AIFF/MP3 for this level.");
        addChildComponent (U.load);
        U.load.onClick = [this, i]
        {
            chooser = std::make_unique<juce::FileChooser> ("Load sample for level " + juce::String (i + 1), juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, i] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f.existsAsFile() && proc.loadSampleForLane (i, f))
                        lanes[(size_t) i].sampleName.setText (f.getFileName(), juce::dontSendNotification);
                });
        };
        U.sampleName.setColour (juce::Label::textColourId, TH.plumSoft);
        U.sampleName.setFont (lnf.pixel (9.0f));
        U.sampleName.setText (juce::File (proc.getSamplePathForLane (i)).getFileName(), juce::dontSendNotification);
        addChildComponent (U.sampleName);
        setupKnob (U.sPitch, id ("spitch"), "PITCH", "Repitch the sample in semitones (ignored when Fit is on).", true);
        setupKnob (U.sStart, id ("sstart"), "START", "Where playback starts. Or drag on the waveform.", false);
        setupKnob (U.sEnd, id ("send"), "END", "Where playback stops. Or drag on the waveform.", false);
        U.sRev.setTooltip ("Play the sample backwards so it lands on the one.");
        U.sFit.setTooltip ("Resample so the selection fits the window exactly - it ends precisely on the one.");
        addChildComponent (U.sRev); addChildComponent (U.sFit);
        U.sRevAtt = std::make_unique<BA> (proc.apvts, id ("srev"), U.sRev);
        U.sFitAtt = std::make_unique<BA> (proc.apvts, id ("sfit"), U.sFit);
        U.wave = std::make_unique<SampleView> (proc, i);
        addChildComponent (*U.wave);

        U.inspectorComps = { &U.unit, &U.place, &U.mode, &U.nudge.slider, &U.nudge.label, &U.group.slider, &U.group.label,
                             &U.count.slider, &U.count.label, &U.every.slider, &U.every.label, &U.len.slider, &U.len.label,
                             &U.prob.slider, &U.prob.label, &U.mix.slider, &U.mix.label, &U.gain.slider, &U.gain.label, &U.clen.slider, &U.clen.label,
                             &U.p1.slider, &U.p1.label, &U.p2.slider, &U.p2.label, &U.p3.slider, &U.p3.label, &U.p4.slider, &U.p4.label,
                             &U.p1b.slider, &U.p1b.label, &U.p2b.slider, &U.p2b.label, &U.p3b.slider, &U.p3b.label, &U.p4b.slider, &U.p4b.label,
                             &U.p1c.slider, &U.p1c.label, &U.p2c.slider, &U.p2c.label, &U.p3c.slider, &U.p3c.label, &U.p4c.slider, &U.p4c.label };
        U.sampleComps = { &U.load, &U.sampleName, &U.sPitch.slider, &U.sPitch.label, &U.sStart.slider, &U.sStart.label,
                          &U.sEnd.slider, &U.sEnd.label, &U.sRev, &U.sFit, U.wave.get() };
        updateHints (i);
    }

    setSize (1140, 900);
    applyWidgetTheme();
    selectLane (0);
    startTimerHz (30);
}

// re-apply theme colours to widgets that captured them at construction
void SofGlitchEditor::applyWidgetTheme()
{
    title.setColour (juce::Label::textColourId, TH.plum);
    transport.setColour (juce::Label::textColourId, TH.plumSoft);
    inspectorTitle.setColour (juce::Label::textColourId, TH.plum);
    auto themeKnob = [] (Knob& k) { k.label.setColour (juce::Label::textColourId, TH.plumSoft); k.slider.setColour (juce::Slider::textBoxTextColourId, TH.plum); };
    for (auto* k : { &masterMix, &seed, &phase, &intBpm }) themeKnob (*k);
    for (auto& U : lanes)
    {
        for (auto* k : { &U.nudge, &U.group, &U.count, &U.every, &U.len, &U.prob, &U.mix, &U.gain,
                         &U.p1, &U.p2, &U.p3, &U.p4, &U.p1b, &U.p2b, &U.p3b, &U.p4b,
                         &U.p1c, &U.p2c, &U.p3c, &U.p4c, &U.sPitch, &U.sStart, &U.sEnd })
            themeKnob (*k);
        U.sampleName.setColour (juce::Label::textColourId, TH.plumSoft);
    }
    // force every child to drop cached LookAndFeel colours
    for (auto* c : getChildren()) c->lookAndFeelChanged();
    lookAndFeelChanged();
}

SofGlitchEditor::~SofGlitchEditor() { setLookAndFeel (nullptr); }

void SofGlitchEditor::updateHints (int lane)
{
    auto& U = lanes[(size_t) lane];
    struct SlotUI { juce::ComboBox* cb; Knob* k[4]; bool withOff; };
    SlotUI slots[3] = { { &U.action, { &U.p1, &U.p2, &U.p3, &U.p4 }, false },
                       { &U.actionB, { &U.p1b, &U.p2b, &U.p3b, &U.p4b }, true },
                       { &U.actionC, { &U.p1c, &U.p2c, &U.p3c, &U.p4c }, true } };
    for (auto& sl : slots)
    {
        const int a = sl.cb->getSelectedItemIndex() - (sl.withOff ? 1 : 0);
        const juce::String hints[4] = { a < 0 ? "" : p1Hint (a), a < 0 ? "" : p2Hint (a),
                                        a < 0 ? "" : p3Hint (a), a < 0 ? "" : p4Hint (a) };
        for (int q = 0; q < 4; ++q)
        {
            sl.k[q]->label.setText (hints[q].toUpperCase(), juce::dontSendNotification);
            sl.k[q]->slider.setTooltip (a < 0 ? "" : actionTip (a));
            sl.k[q]->slider.setColour (juce::Slider::rotarySliderFillColourId, a < 0 ? TH.plumSoft : actionColour (a));
        }
        sl.cb->setTooltip (a < 0 ? "Add another effect to this level's chain." : actionTip (a));
    }
}

void SofGlitchEditor::selectLane (int lane)
{
    selected = juce::jlimit (0, kNumLanes - 1, lane);
    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& U = lanes[(size_t) i];
        for (auto* c : U.inspectorComps) c->setVisible (i == selected);
        for (auto* c : U.sampleComps) c->setVisible (false);
        U.fill.setVisible (false); U.clear.setVisible (false);
    }
    layoutInspector (selected);
    repaint();
}

// ---------------------------------------------------------------- track stack geometry
int SofGlitchEditor::laneRowHeight (int lane) const { return kRowH + (expanded[lane] ? 2 * kSubH : 0); }
int SofGlitchEditor::laneRowTop (int lane) const
{
    int y = 0;
    for (int i = 0; i < lane; ++i) y += laneRowHeight (i);
    return y;
}
juce::Rectangle<int> SofGlitchEditor::laneMainRow (int lane) const
{
    return { timelineArea.getX(), timelineArea.getY() + kRulerH + laneRowTop (lane), timelineArea.getWidth(), kRowH };
}
juce::Rectangle<int> SofGlitchEditor::laneFullRow (int lane) const
{
    return { timelineArea.getX(), timelineArea.getY() + kRulerH + laneRowTop (lane), timelineArea.getWidth(), laneRowHeight (lane) };
}

int SofGlitchEditor::visibleBars() const
{
    switch (zoom.getSelectedId()) { case 1: return 4; case 3: return 16; case 4: return 32; default: return 8; }
}

void SofGlitchEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! timelineArea.contains (e.getPosition())) return;
    const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY;
    if (d == 0.0f) return;
    wheelAccum += d * 3.0f;                       // ~1 bar per notch, smooth on trackpads
    const int step = (int) wheelAccum;
    if (step != 0)
    {
        wheelAccum -= (float) step;
        viewStartBar = juce::jlimit (0, 2000, viewStartBar - step);
        lastManualScroll = juce::Time::getMillisecondCounter();
        repaint();
    }
}

void SofGlitchEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumLanes; ++i)
    {
        if (! laneFullRow (i).contains (e.getPosition())) continue;
        selectLane (i);
        auto& U = lanes[(size_t) i];
        const auto grid = timelineArea.withTrimmedLeft (kHeaderW);
        if (U.mode.getSelectedItemIndex() != 1 || ! grid.contains (e.getPosition())) return;
        const double bpb = 4.0;
        const double visBeats = visibleBars() * bpb;
        const double beat = viewStartBar * bpb + (e.x - grid.getX()) / (double) grid.getWidth() * visBeats;
        const double unit = unitBeats (U.unit.getSelectedItemIndex(), bpb);
        const int group = juce::jmax (2, (int) U.group.slider.getValue());
        const int count = juce::jlimit (1, group - 1, (int) U.count.slider.getValue());
        const double cycleLen = unit * group;
        const double startBeat = windowStartBeats (U.place.getSelectedItemIndex(), unit, group, count, (float) U.nudge.slider.getValue());
        const double phaseBeats = proc.apvts.getRawParameterValue ("phase")->load() * bpb;
        const double winLen = count * unit * U.len.slider.getValue();
        const int clen = juce::jmax (1, (int) U.clen.slider.getValue());
        const long cycleRaw = (long) std::round ((beat - phaseBeats - startBeat - winLen * 0.5) / cycleLen);
        const long cycle = ((cycleRaw % clen) + clen) % clen;
        proc.setCustom (i, cycle, ! proc.getCustom (i, cycle));
        repaint();
        return;
    }
}

void SofGlitchEditor::timerCallback()
{
    const double ppq = proc.displayPpq.load();
    const int bar = (int) std::floor (ppq / 4.0) + 1;
    const int beat = (int) std::floor (std::fmod (std::abs (ppq), 4.0)) + 1;
    transport.setText (juce::String (proc.hostPlaying.load() ? "PLAYING " : "STOPPED ") + juce::String (proc.displayBpm.load(), 1) + " BPM  "
                       + juce::String (bar) + "." + juce::String (beat), juce::dontSendNotification);
    repaint (timelineArea);
}

// ---------------------------------------------------------------- painting
void SofGlitchEditor::drawBackground (juce::Graphics& g)
{
    juce::ColourGradient grad (TH.skyTop, 0, 0, TH.skyBot, 0, (float) getHeight(), false);
    grad.addColour (0.45, TH.skyMid);
    g.setGradientFill (grad);
    g.fillAll();

    // deterministic sparkles
    juce::Random rng (7);
    for (int i = 0; i < 26; ++i)
    {
        const float x = rng.nextFloat() * getWidth(), y = rng.nextFloat() * getHeight();
        const float s = 1.5f + rng.nextFloat() * 1.5f;
        pxBitmap (g, kSparkle, 5, x, y, s, juce::Colours::white.withAlpha (TH.sparkAlpha * (0.6f + rng.nextFloat() * 0.5f)));
    }
    // pixel clouds
    pxBitmap (g, kCloud, 6, -18.0f, getHeight() - 64.0f, 7.0f, juce::Colours::white.withAlpha (TH.cloudAlpha));
    pxBitmap (g, kCloud, 6, getWidth() - 132.0f, getHeight() - 50.0f, 6.0f, juce::Colours::white.withAlpha (TH.cloudAlpha * 0.92f));
    pxBitmap (g, kCloud, 6, getWidth() - 210.0f, 6.0f, 4.0f, juce::Colours::white.withAlpha (TH.cloudAlpha * 0.75f));
    pxBitmap (g, kCloud, 6, 40.0f, 10.0f, 3.0f, juce::Colours::white.withAlpha (TH.cloudAlpha * 0.65f));

    // dithered checker border frame
    const int cs = 8;
    for (int x = 0; x < getWidth(); x += cs)
        for (int side = 0; side < 2; ++side)
        {
            const int y = side == 0 ? 0 : getHeight() - cs;
            g.setColour (((x / cs) & 1) == 0 ? TH.chkA : TH.chkB);
            g.fillRect (x, y, cs, cs);
        }
    for (int y = 0; y < getHeight(); y += cs)
        for (int side = 0; side < 2; ++side)
        {
            const int x = side == 0 ? 0 : getWidth() - cs;
            g.setColour (((y / cs) & 1) == 0 ? TH.chkB : TH.chkA);
            g.fillRect (x, y, cs, cs);
        }
}

void SofGlitchEditor::drawTimeline (juce::Graphics& g)
{
    const int bars = visibleBars();
    const double bpb = 4.0;
    const double visBeats = bars * bpb;
    // follow the playhead: only snap pages when it actually leaves the view (and never go negative)
    const double ppqNow = juce::jmax (0.0, proc.displayPpq.load());
    const bool scrolledRecently = juce::Time::getMillisecondCounter() - lastManualScroll < 2000;
    if (proc.hostPlaying.load() && ! scrolledRecently)
    {
        const double v0now = viewStartBar * bpb;
        if (ppqNow < v0now || ppqNow >= v0now + visBeats)
            viewStartBar = juce::jmax (0, (int) std::floor (ppqNow / visBeats) * bars);
    }
    const double v0 = viewStartBar * bpb;
    const auto grid = timelineArea.withTrimmedLeft (kHeaderW);
    const double pxPerBeat = grid.getWidth() / visBeats;
    auto xOf = [&] (double beat) { return (float) (grid.getX() + (beat - v0) * pxPerBeat); };

    bevelCard (g, timelineArea.toFloat(), TH.ivory, 6.0f);
    const int rowsBottom = laneFullRow (kNumLanes - 1).getBottom();

    // ruler: lavender strip with pixel ticks
    auto ruler = juce::Rectangle<int> (grid.getX(), timelineArea.getY() + 2, grid.getWidth() - 2, kRulerH - 2);
    g.setColour (TH.chkA); g.fillRect (ruler);
    g.setFont (lnf.pixel (9.0f));
    for (int b = 0; b < (int) visBeats; ++b)
    {
        const bool isBar = b % 4 == 0;
        const float x = xOf (v0 + b);
        g.setColour (isBar ? TH.plum : TH.plumSoft.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, (float) (isBar ? ruler.getY() : ruler.getCentreY()), (float) rowsBottom);
        if (isBar)
        {
            g.setColour (TH.plum);
            g.drawText (juce::String (viewStartBar + b / 4 + 1), (int) x + 4, ruler.getY(), 40, ruler.getHeight(), juce::Justification::centredLeft);
        }
    }

    const uint32_t seedV = (uint32_t) proc.apvts.getRawParameterValue ("seed")->load();
    const double phaseBeats = proc.apvts.getRawParameterValue ("phase")->load() * bpb;

    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& U = lanes[(size_t) i];
        const auto full = laneFullRow (i);
        const auto main = laneMainRow (i);
        if (i == selected) { g.setColour (TH.chkB.withAlpha (0.7f)); g.fillRect (full.reduced (2, 0)); }
        g.setColour (TH.plumSoft.withAlpha (0.35f));
        g.drawHorizontalLine (full.getBottom() - 1, (float) full.getX() + 4.0f, (float) full.getRight() - 4.0f);

        const bool on = U.on.getToggleState();
        const double unit = unitBeats (U.unit.getSelectedItemIndex(), bpb);
        const int group = juce::jmax (2, (int) U.group.slider.getValue());
        const int count = juce::jlimit (1, group - 1, (int) U.count.slider.getValue());
        const int every = juce::jmax (1, (int) U.every.slider.getValue());
        const double len = U.len.slider.getValue();
        const double prob = U.prob.slider.getValue();
        const double cycleLen = unit * group;
        const double startBeat = windowStartBeats (U.place.getSelectedItemIndex(), unit, group, count, (float) U.nudge.slider.getValue());
        const bool custom = U.mode.getSelectedItemIndex() == 1;
        const int clen = juce::jmax (1, (int) U.clen.slider.getValue());

        // slot rows: main row = slot A; expanded sub-rows = B and C
        struct SlotDraw { int action; juce::Rectangle<int> row; bool active; };
        SlotDraw sd[3];
        sd[0] = { U.action.getSelectedItemIndex(), main.withTrimmedLeft (kHeaderW).reduced (0, 9), true };
        sd[1] = { U.actionB.getSelectedItemIndex() - 1,
                  juce::Rectangle<int> (grid.getX(), main.getBottom(), grid.getWidth(), kSubH).reduced (0, 5),
                  U.actionB.getSelectedItemIndex() > 0 };
        sd[2] = { U.actionC.getSelectedItemIndex() - 1,
                  juce::Rectangle<int> (grid.getX(), main.getBottom() + kSubH, grid.getWidth(), kSubH).reduced (0, 5),
                  U.actionC.getSelectedItemIndex() > 0 };

        const long c0 = (long) std::floor ((v0 - phaseBeats - startBeat) / cycleLen) - 1;
        const long c1 = (long) std::floor ((v0 + visBeats - phaseBeats - startBeat) / cycleLen) + 1;
        if (c1 - c0 > 4096) continue;
        for (long c = c0; c <= c1; ++c)
        {
            if (! custom && ((c % every) + every) % every != every - 1) continue;
            const double start = c * cycleLen + startBeat + phaseBeats;
            const double winLen = count * unit * len;
            if (start + winLen < v0 || start > v0 + visBeats) continue;
            const bool fires = custom ? proc.getCustom (i, ((c % clen) + clen) % clen) : laneRoll (c, i, seedV) < prob;
            const float x0 = xOf (juce::jmax (v0, start)), x1 = xOf (juce::jmin (v0 + visBeats, start + winLen));
            const int nRows = expanded[i] ? 3 : 1;
            for (int sri = 0; sri < nRows; ++sri)
            {
                if (! sd[sri].active) continue;
                const auto col = actionColour (sd[sri].action).withMultipliedAlpha (on ? 1.0f : 0.35f);
                auto rr = juce::Rectangle<float> (x0, (float) sd[sri].row.getY(), juce::jmax (2.0f, x1 - x0), (float) sd[sri].row.getHeight());
                if (fires)
                {
                    g.setColour (col); g.fillRect (rr);
                    g.setColour (col.darker (0.5f)); g.drawRect (rr, 1.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.5f));
                    g.fillRect (rr.getX() + 1.0f, rr.getY() + 1.0f, juce::jmax (0.0f, rr.getWidth() - 2.0f), 1.5f);
                }
                else
                {
                    g.setColour (col.withMultipliedAlpha (0.18f)); g.fillRect (rr);
                    g.setColour (col.withMultipliedAlpha (0.55f)); g.drawRect (rr, 1.0f);
                }
            }
        }
        // heart LED
        const bool lit = proc.laneActivity[i].load() > 0.5f;
        pxBitmap (g, kHeart, 5, (float) full.getX() + 8.0f, (float) main.getCentreY() - 5.0f, 2.0f,
                  lit ? actionColour (sd[0].action) : TH.plumSoft.withAlpha (0.4f));
        // sub-row labels in the header zone
        if (expanded[i])
        {
            g.setFont (lnf.pixel (9.0f));
            g.setColour (TH.plumSoft);
            g.drawText ("B", full.getX() + 26, sd[1].row.getY() - 5, 14, kSubH, juce::Justification::centredLeft);
            g.drawText ("C", full.getX() + 26, sd[2].row.getY() - 5, 14, kSubH, juce::Justification::centredLeft);
        }
    }

    // header divider
    g.setColour (TH.plum.withAlpha (0.5f));
    g.drawVerticalLine (grid.getX() - 1, (float) timelineArea.getY() + 2.0f, (float) rowsBottom);

    // playhead: deep pink with a pixel diamond cap
    const double pos = proc.displayPpq.load() - v0;
    if (pos >= 0 && pos <= visBeats)
    {
        const float x = xOf (v0 + pos);
        g.setColour (juce::Colour (0xffe75c9d));
        g.drawLine (x, (float) ruler.getY(), x, (float) rowsBottom, 2.0f);
        g.fillRect (x - 3.0f, (float) ruler.getY(), 6.0f, 3.0f);
        g.fillRect (x - 1.5f, (float) ruler.getY() + 3.0f, 3.0f, 3.0f);
    }
}

void SofGlitchEditor::paint (juce::Graphics& g)
{
    drawBackground (g);
    // top bar card
    bevelCard (g, juce::Rectangle<float> (10, 10, (float) getWidth() - 20, 62), TH.ivory, 6.0f);
    drawTimeline (g);
    bevelCard (g, inspectorArea.toFloat(), TH.ivory, 6.0f);
    g.setColour (actionColour (lanes[(size_t) selected].action.getSelectedItemIndex()));
    g.fillRect (inspectorArea.getX() + 3, inspectorArea.getY() + 6, 4, inspectorArea.getHeight() - 12);
    g.setColour (TH.plum.withAlpha (0.8f));
    g.setFont (lnf.pixel (8.0f));
    g.drawText ("click a track to edit - solid clip = fires, ghost = off - CUSTOM mode: click clips to toggle - wheel scrolls - top level wins on overlap - hover for help",
                12, getHeight() - 22, getWidth() - 24, 14, juce::Justification::centredLeft);
}

void SofGlitchEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto top = area.removeFromTop (62).reduced (8, 4);
    title.setBounds (top.removeFromLeft (280).withHeight (34).withY (top.getY() + 12));
    transport.setBounds (top.removeFromRight (220).withHeight (26).withY (top.getY() + 16));
    auto placeKnob = [] (Knob& k, juce::Rectangle<int> r) { k.label.setBounds (r.removeFromBottom (12)); k.slider.setBounds (r); };
    auto gl = top.withTrimmedLeft (6);
    placeKnob (masterMix, gl.removeFromLeft (58));
    placeKnob (seed, gl.removeFromLeft (58));
    placeKnob (phase, gl.removeFromLeft (58));
    placeKnob (intBpm, gl.removeFromLeft (58));
    freeRun.setBounds (gl.removeFromLeft (86).withSizeKeepingCentre (86, 22));
    zoom.setBounds (gl.removeFromLeft (96).withSizeKeepingCentre (92, 22));
    gl.removeFromLeft (6);
    darkToggle.setBounds (gl.removeFromLeft (76).withSizeKeepingCentre (76, 22));

    area.removeFromTop (8);
    area.removeFromBottom (20);
    inspectorArea = area.removeFromBottom (256);
    area.removeFromBottom (8);
    int totalRows = 0; for (int i = 0; i < kNumLanes; ++i) totalRows += laneRowHeight (i);
    timelineArea = area.withHeight (juce::jmin (area.getHeight(), kRulerH + totalRows + 6));

    layoutTracks();
    layoutInspector (selected);
}

void SofGlitchEditor::layoutTracks()
{
    int totalRows = 0; for (int i = 0; i < kNumLanes; ++i) totalRows += laneRowHeight (i);
    timelineArea = timelineArea.withHeight (kRulerH + totalRows + 6);
    for (int i = 0; i < kNumLanes; ++i)
    {
        auto& U = lanes[(size_t) i];
        auto main = laneMainRow (i);
        auto h = main.withWidth (kHeaderW).reduced (0, 7).withTrimmedLeft (22);
        auto row1 = h.removeFromTop (18);
        U.disclose.setBounds (row1.removeFromLeft (18));
        U.dup.setBounds (row1.removeFromRight (38).reduced (1));
        U.on.setBounds (row1.reduced (2, 0));
        h.removeFromTop (2);
        auto row2 = h;
        U.unit.setBounds (row2.removeFromLeft (64).withHeight (20));
        row2.removeFromLeft (4);
        U.action.setBounds (row2.withHeight (20).withTrimmedRight (10));
        // sub-row combos
        U.actionB.setVisible (expanded[i]);
        U.actionC.setVisible (expanded[i]);
        if (expanded[i])
        {
            U.actionB.setBounds (main.getX() + 40, main.getBottom() + 1, kHeaderW - 56, kSubH - 5);
            U.actionC.setBounds (main.getX() + 40, main.getBottom() + kSubH + 1, kHeaderW - 56, kSubH - 5);
        }
    }
}

void SofGlitchEditor::layoutInspector (int lane)
{
    auto& U = lanes[(size_t) lane];
    const int a = U.action.getSelectedItemIndex();
    const int ab = U.actionB.getSelectedItemIndex() - 1;
    const int ac = U.actionC.getSelectedItemIndex() - 1;
    juce::String chain = actionNames()[a];
    if (ab >= 0) chain << " > " << actionNames()[ab];
    if (ac >= 0) chain << " > " << actionNames()[ac];
    const bool before = U.place.getSelectedItemIndex() == 0;
    const juce::String cyc = juce::String ((int) U.group.slider.getValue()) + " x " + unitNames()[U.unit.getSelectedItemIndex()];
    inspectorTitle.setText ("L" + juce::String (lane + 1) + " - " + chain + " - "
                            + (before ? "BEFORE the one: last " + juce::String ((int) U.count.slider.getValue()) + " of " + cyc
                                      : "ON the one: first " + juce::String ((int) U.count.slider.getValue()) + " of " + cyc),
                            juce::dontSendNotification);
    auto r = inspectorArea.reduced (16, 8);
    inspectorTitle.setBounds (r.removeFromTop (18));
    auto placeKnob = [] (Knob& k, juce::Rectangle<int> rr) { k.label.setBounds (rr.removeFromBottom (12)); k.slider.setBounds (rr); };
    const int kw = 58;

    // row 1: timing
    auto row1 = r.removeFromTop (76);
    auto leftCol = row1.removeFromLeft (128);
    U.place.setBounds (leftCol.removeFromTop (22).withY (leftCol.getY() + 4));
    U.mode.setBounds (leftCol.removeFromTop (22).withY (leftCol.getY() + 8));
    row1.removeFromLeft (8);
    placeKnob (U.nudge, row1.removeFromLeft (kw));
    placeKnob (U.group, row1.removeFromLeft (kw));
    placeKnob (U.count, row1.removeFromLeft (kw));
    placeKnob (U.every, row1.removeFromLeft (kw));
    row1.removeFromLeft (10);
    placeKnob (U.len, row1.removeFromLeft (kw));
    placeKnob (U.prob, row1.removeFromLeft (kw));
    row1.removeFromLeft (10);
    placeKnob (U.mix, row1.removeFromLeft (kw));
    placeKnob (U.gain, row1.removeFromLeft (kw));
    const bool custom = U.mode.getSelectedItemIndex() == 1;
    U.fill.setVisible (custom && lane == selected); U.clear.setVisible (custom && lane == selected);
    U.clen.slider.setVisible (custom && lane == selected);
    U.clen.label.setVisible (custom && lane == selected);
    if (custom)
    {
        row1.removeFromLeft (10);
        placeKnob (U.clen, row1.removeFromLeft (kw));
        row1.removeFromLeft (6);
        auto col = row1.removeFromLeft (104);
        U.fill.setBounds (col.removeFromTop (22).withY (col.getY() + 8));
        U.clear.setBounds (col.removeFromTop (22).withY (col.getY() + 12));
    }
    for (auto* k : { &U.prob, &U.every }) { k->slider.setAlpha (custom ? 0.35f : 1.0f); k->label.setAlpha (custom ? 0.35f : 1.0f); }

    // row 2: the fx chain knobs, grouped A / B / C
    auto row2 = r.removeFromTop (76);
    struct SlotK { int act; Knob* k[4]; };
    SlotK sk[3] = { { a, { &U.p1, &U.p2, &U.p3, &U.p4 } }, { ab, { &U.p1b, &U.p2b, &U.p3b, &U.p4b } }, { ac, { &U.p1c, &U.p2c, &U.p3c, &U.p4c } } };
    for (int sidx = 0; sidx < 3; ++sidx)
    {
        const bool act = sidx == 0 || sk[sidx].act >= 0;
        for (int q = 0; q < 4; ++q)
        {
            sk[sidx].k[q]->slider.setVisible (act && lane == selected);
            sk[sidx].k[q]->label.setVisible (act && lane == selected);
            if (act) placeKnob (*sk[sidx].k[q], row2.removeFromLeft (kw));
        }
        if (act) row2.removeFromLeft (16);
    }

    // row 3: sample
    auto row3 = r;
    const bool isSample = a == Sample || ab == Sample || ac == Sample;
    for (auto* c : U.sampleComps) c->setVisible (isSample && lane == selected);
    if (isSample)
    {
        auto col = row3.removeFromLeft (130);
        U.load.setBounds (col.removeFromTop (22).withY (col.getY() + 2));
        U.sampleName.setBounds (col.removeFromTop (14).withY (col.getY() + 4));
        U.sRev.setBounds (col.removeFromTop (18).withY (col.getY() + 6));
        U.sFit.setBounds (col.removeFromTop (18).withY (col.getY() + 8));
        row3.removeFromLeft (6);
        placeKnob (U.sPitch, row3.removeFromLeft (kw));
        placeKnob (U.sStart, row3.removeFromLeft (kw));
        placeKnob (U.sEnd, row3.removeFromLeft (kw));
        row3.removeFromLeft (10);
        U.wave->setBounds (row3.reduced (0, 4));
    }
}
