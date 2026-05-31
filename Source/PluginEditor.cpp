#include "PluginEditor.h"

//==============================================================================
// Colours pulled from the photo of the pedal.
namespace PF
{
    const juce::Colour purpleTop   { 0xff8a5cc4 };
    const juce::Colour purpleBot   { 0xff4e2f86 };
    const juce::Colour gold         { 0xffe8c860 };
    const juce::Colour goldDull     { 0xffb59a4e };  // tarnished gold for faded print
    const juce::Colour ivory        { 0xfff2ead8 };
    const juce::Colour ivoryAged    { 0xffe6d9b8 };  // yellowed ivory knob
    const juce::Colour ivoryShade   { 0xffbfb495 };
    const juce::Colour darkRing     { 0xff2a1d40 };
    const juce::Colour textWhite    { 0xfff6f2ff };
    const juce::Colour textWorn     { 0xffd8d0e4 };
}

//==============================================================================
ProFuzzLookAndFeel::ProFuzzLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,    PF::textWhite);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,            PF::textWhite);
}

void ProFuzzLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                           float pos, float startAngle, float endAngle,
                                           juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const auto cx = bounds.getCentreX();
    const auto cy = bounds.getCentreY();
    const auto r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto angle = startAngle + pos * (endAngle - startAngle);

    // grimy finger-dirt ring worn into the paint around the knob
    {
        juce::ColourGradient ring (juce::Colours::transparentBlack, cx, cy,
                                   juce::Colours::black.withAlpha (0.18f), cx, cy + r * 1.5f, true);
        ring.addColour (0.72, juce::Colours::transparentBlack);
        ring.addColour (0.92, juce::Colours::black.withAlpha (0.16f));
        g.setGradientFill (ring);
        g.fillEllipse (cx - r * 1.5f, cy - r * 1.5f, r * 3.0f, r * 3.0f);
    }

    // soft drop shadow under the knob
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - r + 2.0f, cy - r + 3.0f, r * 2.0f, r * 2.0f);

    // dark base ring
    g.setColour (PF::darkRing);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    // aged ivory cap: yellowed, vertical shade gradient (dingier than new)
    const auto cr = r * 0.82f;
    juce::ColourGradient grad (PF::ivoryAged, cx, cy - cr, PF::ivoryShade, cx, cy + cr, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);

    // grime smudge on the cap, deterministic per knob (seed from position)
    {
        juce::Random rng ((int) (cx * 13.0f + cy * 7.0f) + 101);
        g.setColour (juce::Colours::black.withAlpha (0.06f));
        for (int i = 0; i < 5; ++i)
        {
            const float a  = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const float rad= rng.nextFloat() * cr * 0.6f;
            const float sz = cr * (0.12f + rng.nextFloat() * 0.18f);
            g.fillEllipse (cx + std::cos (a) * rad - sz * 0.5f,
                           cy + std::sin (a) * rad - sz * 0.5f, sz, sz);
        }
    }

    // a couple of fine scratches across the cap
    {
        juce::Random rng ((int) (cx * 5.0f + cy * 11.0f) + 7);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        for (int i = 0; i < 3; ++i)
        {
            const float a  = rng.nextFloat() * juce::MathConstants<float>::pi;
            const float len= cr * (0.7f + rng.nextFloat() * 0.6f);
            const float ox = (rng.nextFloat() - 0.5f) * cr;
            const float oy = (rng.nextFloat() - 0.5f) * cr;
            juce::Line<float> ln (cx + ox - std::cos (a) * len * 0.5f,
                                  cy + oy - std::sin (a) * len * 0.5f,
                                  cx + ox + std::cos (a) * len * 0.5f,
                                  cy + oy + std::sin (a) * len * 0.5f);
            g.drawLine (ln, 0.7f);
        }
    }

    // worn rim highlight + shade
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f, 1.0f);
    g.setColour (PF::darkRing.withAlpha (0.5f));
    g.drawEllipse (cx - cr + 1.0f, cy - cr + 1.0f, cr * 2.0f - 2.0f, cr * 2.0f - 2.0f, 1.2f);

    // pointer notch (slightly grimy)
    juce::Path p;
    const float pw = cr * 0.16f;
    p.addRoundedRectangle (-pw * 0.5f, -cr * 0.95f, pw, cr * 0.7f, pw * 0.4f);
    g.setColour (PF::darkRing);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (cx, cy));

    juce::ignoreUnused (s);
}

//==============================================================================
void FootSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX();
    const bool  on = getToggleState();   // Mk II engaged

    // --- LED above the switch ---
    const float ledY = b.getY() + 14.0f;
    const float lr = 7.0f;
    if (on)
    {
        juce::ColourGradient glow (juce::Colour (0xffff5a4a).withAlpha (0.55f), cx, ledY,
                                   juce::Colours::transparentBlack, cx, ledY + 22.0f, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - 22.0f, ledY - 22.0f, 44.0f, 44.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - lr - 1.5f, ledY - lr - 1.5f, (lr + 1.5f) * 2.0f, (lr + 1.5f) * 2.0f);
    g.setColour (on ? juce::Colour (0xffff3b30) : juce::Colour (0xff5a1410));
    g.fillEllipse (cx - lr, ledY - lr, lr * 2.0f, lr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.8f : 0.2f)); // hot-spot
    g.fillEllipse (cx - lr * 0.4f, ledY - lr * 0.6f, lr * 0.6f, lr * 0.6f);

    // --- chrome stomp button ---
    const float sy = b.getY() + 56.0f;
    const float sr = 24.0f;
    const float press = down ? 1.5f : 0.0f;

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (cx - sr - 3.0f, sy - sr - 1.0f + press, (sr + 3.0f) * 2.0f, (sr + 3.0f) * 2.0f);

    juce::ColourGradient chrome (juce::Colour (0xffe9ecf2), cx, sy - sr + press,
                                 juce::Colour (0xff7c8090), cx, sy + sr + press, false);
    chrome.addColour (0.5, juce::Colour (0xffb9bfcc));
    g.setGradientFill (chrome);
    g.fillEllipse (cx - sr, sy - sr + press, sr * 2.0f, sr * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.5f : 0.3f));
    g.drawEllipse (cx - sr + 2.0f, sy - sr + 2.0f + press, sr * 2.0f - 4.0f, sr * 2.0f - 4.0f, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (cx - sr * 0.45f, sy - sr * 0.45f + press, sr * 0.9f, sr * 0.9f);

    // --- Mk I / Mk II labels flanking the stomp; active one lit ---
    const juce::Colour gold { 0xffe8c860 };
    const juce::Colour dim  { 0xff7a7488 };
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.setColour (on ? dim : gold);
    g.drawText ("Mk I",  juce::Rectangle<float> (b.getX(), sy - 10.0f, cx - sr - b.getX() - 4.0f, 20.0f),
                juce::Justification::centredRight, false);
    g.setColour (on ? gold : dim);
    g.drawText ("Mk II", juce::Rectangle<float> (cx + sr + 4.0f, sy - 10.0f, b.getRight() - (cx + sr) - 4.0f, 20.0f),
                juce::Justification::centredLeft, false);
}

//==============================================================================
ProFuzzAudioProcessorEditor::ProFuzzAudioProcessorEditor (ProFuzzAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);

    // The three real Pro Fuzz controls (big knobs).
    setupKnob (level, "level", "MASTER", true);
    setupKnob (tone,  "tone",  "TONE",   true);
    setupKnob (drive, "drive", "FUZZ",   true);

    // Extras (small knobs).
    setupKnob (dying,  "dying",  "Dying",  false);
    setupKnob (warmth, "warmth", "Warmth", false);
    setupKnob (bias,   "bias",   "Bias",   false);
    setupKnob (gate,   "gate",   "Gate",   false);
    setupKnob (mix,    "mix",    "Mix",    false);

    // Dying-flavor selector (combo box).
    dyModeBox.addItemList ({ "Bias Drift", "Sputter", "Cap Leak" }, 1);
    dyModeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a1d40));
    dyModeBox.setColour (juce::ComboBox::textColourId,       PF::textWorn);
    dyModeBox.setColour (juce::ComboBox::outlineColourId,    PF::goldDull.withAlpha (0.6f));
    dyModeBox.setColour (juce::ComboBox::arrowColourId,      PF::goldDull);
    addAndMakeVisible (dyModeBox);
    dyModeAttach = std::make_unique<ComboAttachment> (processor.apvts, "dymode", dyModeBox);

    dyModeLabel.setText ("DYING MODE", juce::dontSendNotification);
    dyModeLabel.setJustificationType (juce::Justification::centred);
    dyModeLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    dyModeLabel.setColour (juce::Label::textColourId, PF::textWorn);
    addAndMakeVisible (dyModeLabel);

    // Mk I / Mk II voicing footswitch.
    addAndMakeVisible (footSw);
    footAttach = std::make_unique<ButtonAttachment> (processor.apvts, "voicing", footSw);

    setSize (380, 760);
}

ProFuzzAudioProcessorEditor::~ProFuzzAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ProFuzzAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramID,
                                             const juce::String& text, bool big)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                              big ? 64 : 48, big ? 18 : 14);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (big ? 15.0f : 11.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<SliderAttachment> (processor.apvts, paramID, k.slider);
}

//==============================================================================
void ProFuzzAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float W = b.getWidth(), H = b.getHeight();

    // purple enclosure with a top-to-bottom gradient
    g.setGradientFill (juce::ColourGradient (PF::purpleTop, 0, 0,
                                             PF::purpleBot, 0, H, false));
    g.fillAll();

    // --- AGEING LAYER 1: uneven paint fade (lighter sun-worn patches) ---
    {
        juce::Random rng (1234);
        for (int i = 0; i < 14; ++i)
        {
            const float px = rng.nextFloat() * W;
            const float py = rng.nextFloat() * H;
            const float sz = 30.0f + rng.nextFloat() * 90.0f;
            g.setColour (juce::Colours::white.withAlpha (0.015f + rng.nextFloat() * 0.03f));
            g.fillEllipse (px - sz * 0.5f, py - sz * 0.5f, sz, sz);
        }
    }

    // --- AGEING LAYER 2: ground-in dirt/grime in patches (darker) ---
    {
        juce::Random rng (99);
        for (int i = 0; i < 16; ++i)
        {
            const float px = rng.nextFloat() * W;
            const float py = rng.nextFloat() * H;
            const float sz = 20.0f + rng.nextFloat() * 70.0f;
            g.setColour (juce::Colours::black.withAlpha (0.02f + rng.nextFloat() * 0.05f));
            g.fillEllipse (px - sz * 0.5f, py - sz * 0.5f, sz, sz);
        }
    }

    // --- AGEING LAYER 3: scratches (fine bright + dark scuff lines) ---
    {
        juce::Random rng (2024);
        for (int i = 0; i < 22; ++i)
        {
            const float x1 = rng.nextFloat() * W;
            const float y1 = rng.nextFloat() * H;
            const float a  = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            const float len= 8.0f + rng.nextFloat() * 55.0f;
            const float x2 = x1 + std::cos (a) * len;
            const float y2 = y1 + std::sin (a) * len;
            const bool bright = rng.nextBool();
            g.setColour (bright ? juce::Colours::white.withAlpha (0.06f + rng.nextFloat() * 0.06f)
                                : juce::Colours::black.withAlpha (0.06f + rng.nextFloat() * 0.08f));
            g.drawLine (x1, y1, x2, y2, 0.5f + rng.nextFloat() * 0.8f);
        }
    }

    // --- corner screws ---
    auto screw = [&g] (float sx, float sy)
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (sx - 6, sy - 5, 13, 13);
        juce::ColourGradient mg (juce::Colour (0xffcfc9d6), sx, sy - 5,
                                 juce::Colour (0xff7a7686), sx, sy + 6, false);
        g.setGradientFill (mg);
        g.fillEllipse (sx - 5, sy - 5, 11, 11);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine (sx - 3.2f, sy - 3.2f, sx + 3.2f, sy + 3.2f, 1.3f); // slot (Phillips-ish)
        g.drawLine (sx + 3.2f, sy - 3.2f, sx - 3.2f, sy + 3.2f, 1.3f);
    };
    screw (16, 16); screw (W - 16, 16); screw (16, H - 16); screw (W - 16, H - 16);

    // --- chipped/worn paint along edges (enclosure metal showing through) ---
    {
        juce::Random rng (555);
        g.setColour (juce::Colour (0xff8f8a9c).withAlpha (0.5f)); // bare aluminium
        for (int i = 0; i < 10; ++i)
        {
            const bool vert = rng.nextBool();
            const float t = rng.nextFloat();
            float px, py;
            if (vert) { px = rng.nextBool() ? 1.0f : W - 1.0f; py = t * H; }
            else      { py = rng.nextBool() ? 1.0f : H - 1.0f; px = t * W; }
            const float sz = 3.0f + rng.nextFloat() * 7.0f;
            juce::Path chip;
            chip.addStar ({ px, py }, 5 + rng.nextInt (3), sz * 0.4f, sz, rng.nextFloat());
            g.fillPath (chip);
        }
    }

    // GFS ELECTRONICS (faded/tarnished gold)
    g.setColour (PF::goldDull);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("GFS", juce::Rectangle<int> (0, 14, getWidth(), 26),
                juce::Justification::centred, false);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("E L E C T R O N I C S", juce::Rectangle<int> (0, 38, getWidth(), 14),
                juce::Justification::centred, false);

    // side jack labels (worn)
    g.setColour (PF::textWorn.withAlpha (0.55f));
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                         14.0f, 240.0f));
        g.drawText ("OUTPUT", juce::Rectangle<int> (-40, 232, 110, 16),
                    juce::Justification::centred, false);
    }
    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi,
                                                         (float) getWidth() - 14.0f, 240.0f));
        g.drawText ("INPUT", juce::Rectangle<int> ((int) getWidth() - 70, 232, 110, 16),
                    juce::Justification::centred, false);
    }

    // big "Pro Fuzz" wordmark + CLASSIC (slightly worn white)
    g.setColour (PF::textWorn);
    g.setFont (juce::Font (46.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("Pro Fuzz", juce::Rectangle<int> (0, 528, getWidth(), 52),
                juce::Justification::centred, false);
    g.setColour (PF::goldDull);
    g.setFont (juce::Font (16.0f, juce::Font::bold));
    g.drawText ("CLASSIC", juce::Rectangle<int> (0, 574, getWidth() - 44, 20),
                juce::Justification::centredRight, false);

    // a worn scuff right across the wordmark (paint rubbed off from handling)
    {
        juce::Random rng (4242);
        g.setColour (PF::purpleTop.withAlpha (0.25f));
        for (int i = 0; i < 4; ++i)
        {
            const float yy = 536.0f + rng.nextFloat() * 40.0f;
            g.drawLine (60.0f + rng.nextFloat() * 40.0f, yy,
                        W - 60.0f - rng.nextFloat() * 40.0f, yy + (rng.nextFloat() - 0.5f) * 6.0f,
                        1.0f + rng.nextFloat() * 1.5f);
        }
    }

    // footer (faded)
    g.setColour (PF::goldDull.withAlpha (0.85f));
    g.setFont (juce::Font (12.0f, juce::Font::italic));
    g.drawText ("Pure Analog  -  True Bypass",
                juce::Rectangle<int> (0, getHeight() - 26, getWidth(), 18),
                juce::Justification::centred, false);

    // --- vignette: darkened, grimy edges (handling dirt builds at the rim) ---
    {
        juce::ColourGradient vig (juce::Colours::transparentBlack, W * 0.5f, H * 0.5f,
                                  juce::Colours::black.withAlpha (0.28f), 0, 0, true);
        vig.addColour (0.65, juce::Colours::transparentBlack);
        g.setGradientFill (vig);
        g.fillRect (b);
    }

    // version badge — clearly readable so the loaded build is unambiguous.
    // Top-right pill under the logo.
    {
        const juce::String ver = "v" JucePlugin_VersionString;
        juce::Rectangle<int> pill (getWidth() - 78, 12, 66, 22);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (pill.toFloat(), 6.0f);
        g.setColour (PF::gold);
        g.drawRoundedRectangle (pill.toFloat(), 6.0f, 1.0f);
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText (ver, pill, juce::Justification::centred, false);
    }
}

void ProFuzzAudioProcessorEditor::resized()
{
    // Knob centres laid out like the pedal face:
    //   Master (top-left), Fuzz (top-right), Tone (centre, lower),
    //   extras in a small row beneath.
    auto place = [] (Knob& k, int cx, int cy, int size, int labelH)
    {
        k.label.setBounds (cx - size/2 - 10, cy - size/2 - labelH, size + 20, labelH);
        k.slider.setBounds (cx - size/2, cy - size/2, size, size + 22);
    };

    const int W = getWidth();

    place (level, W/4,   140, 96, 18);   // MASTER
    place (drive, 3*W/4, 140, 96, 18);   // FUZZ
    place (tone,  W/2,   275, 96, 18);   // TONE

    // five small extras across one row
    Knob* ex[] = { &dying, &warmth, &bias, &gate, &mix };
    for (int i = 0; i < 5; ++i)
        place (*ex[i], (int) ((i + 0.5f) * (W / 5.0f)), 400, 46, 13);

    // dying-mode selector centered below the extras (clear of the knob value boxes)
    dyModeLabel.setBounds (W/2 - 90, 460, 180, 14);
    dyModeBox.setBounds   (W/2 - 80, 476, 160, 24);

    // voicing footswitch near the bottom (below the wordmark)
    footSw.setBounds (W/2 - 90, 612, 180, 100);
}
