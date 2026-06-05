#include "VocalDistortionEditor.h"

//==============================================================================
// Fairchild 670 palette: blued steel panel, cream VU dial, brass accents.
namespace VX
{
    const juce::Colour panelTop  { 0xff5a6b78 };  // blued steel, lighter top
    const juce::Colour panelBot  { 0xff35424c };  // darker bottom
    const juce::Colour rail      { 0xff20272d };  // side rails
    const juce::Colour ink       { 0xff0e1318 };
    const juce::Colour engrave   { 0xffd7dde2 };  // light engraved text
    const juce::Colour brass     { 0xffc7a24e };
    const juce::Colour brassDark { 0xff8a6f33 };
    const juce::Colour dialFace  { 0xfff3ecd6 };  // VU dial cream
    const juce::Colour dialFaceLo{ 0xffe4d8b4 };
    const juce::Colour redLamp   { 0xffcf2a1d };
}

//==============================================================================
VoxLookAndFeel::VoxLookAndFeel()
{
    setColour (juce::Label::textColourId, VX::engrave);
    setColour (juce::Slider::textBoxTextColourId, VX::engrave);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void VoxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float startAngle, float endAngle,
                                       juce::Slider&)
{
    auto area = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();
    const auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto angle = startAngle + pos * (endAngle - startAngle);

    // shadow
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (cx - r + 2.0f, cy - r + 3.0f, r * 2.0f, r * 2.0f);

    // brass collar
    g.setColour (VX::brassDark);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (VX::brass.withAlpha (0.5f));
    g.drawEllipse (cx - r + 1.0f, cy - r + 1.0f, r * 2.0f - 2.0f, r * 2.0f - 2.0f, 1.2f);

    // black bakelite chicken-head body
    const auto capR = r * 0.80f;
    juce::ColourGradient cap (juce::Colour (0xff2c2c30), cx, cy - capR,
                              juce::Colour (0xff080809), cx, cy + capR, false);
    cap.addColour (0.45, juce::Colour (0xff19191c));
    g.setGradientFill (cap);
    g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (cx - capR + 2.0f, cy - capR + 2.0f, capR * 2.0f - 4.0f, capR * 2.0f - 4.0f, 1.0f);

    // chicken-head pointer: a tapered wedge reaching past the cap
    juce::Path beak;
    const float tip = capR * 1.02f;
    const float baseW = capR * 0.30f;
    beak.addTriangle (0.0f, -tip, -baseW * 0.5f, -capR * 0.30f, baseW * 0.5f, -capR * 0.30f);
    g.setColour (VX::engrave);
    g.fillPath (beak, juce::AffineTransform::rotation (angle).translated (cx, cy));

    // hub
    g.setColour (juce::Colour (0xff050506));
    g.fillEllipse (cx - capR * 0.22f, cy - capR * 0.22f, capR * 0.44f, capR * 0.44f);
}

//==============================================================================
VUMeter::VUMeter (DrowningInVoxAudioProcessor& p) : processor (p)
{
    startTimerHz (30);
}

VUMeter::~VUMeter() { stopTimer(); }

void VUMeter::timerCallback()
{
    // Map linear output level to VU deflection. ~0.25 linear (~-12 dBFS avg)
    // reads about 0 VU; clamp and gamma-shape for a natural sweep.
    const float lin = processor.vuLinear.load (std::memory_order_relaxed);
    const float db  = juce::Decibels::gainToDecibels (lin + 1.0e-6f);
    // RMS VU scale. Auto Level targets -18 dBFS RMS, which should read 0 VU near
    // the top of the sweep. Map -30 dBFS -> 0 (left), -6 dBFS -> 1 (right);
    // 0 VU (-18) lands at 0.5, red zone above ~-12 dBFS.
    float target = juce::jmap (db, -30.0f, -6.0f, 0.0f, 1.0f);
    target = juce::jlimit (0.0f, 1.0f, target);

    // moving-coil inertia
    needle += 0.35f * (target - needle);
    repaint();
}

void VUMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // dial recess / bezel
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b, 8.0f);
    auto face = b.reduced (7.0f);

    // cream dial face with subtle vertical shade
    juce::ColourGradient fg (VX::dialFace, face.getCentreX(), face.getY(),
                             VX::dialFaceLo, face.getCentreX(), face.getBottom(), false);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (face, 5.0f);

    // geometry: needle pivots from below the dial, sweeps an arc near the top
    const float cx = face.getCentreX();
    const float pivotY = face.getBottom() + face.getHeight() * 0.62f;
    const float radius = pivotY - face.getY() - 8.0f;
    const float halfSweep = juce::degreesToRadians (29.0f); // +/- from vertical

    auto angleFor = [&] (float t) { return -halfSweep + t * (2.0f * halfSweep); };
    auto ptAt = [&] (float t, float rr)
    {
        const float a = angleFor (t);
        return juce::Point<float> (cx + std::sin (a) * rr, pivotY - std::cos (a) * rr);
    };

    // scale arc + ticks
    for (int i = 0; i <= 10; ++i)
    {
        const float t = (float) i / 10.0f;
        const bool major = (i % 2 == 0);
        const bool red = t > 0.72f;
        auto p1 = ptAt (t, radius);
        auto p2 = ptAt (t, radius - (major ? 11.0f : 6.0f));
        g.setColour (red ? VX::redLamp : VX::ink);
        g.drawLine ({ p1, p2 }, major ? 1.8f : 1.0f);
    }
    // red zone arc near the top-right
    {
        juce::Path redArc;
        const int seg = 16;
        for (int i = 0; i <= seg; ++i)
        {
            const float t = 0.72f + (float) i / seg * 0.28f;
            auto pt = ptAt (t, radius + 3.0f);
            if (i == 0) redArc.startNewSubPath (pt); else redArc.lineTo (pt);
        }
        g.setColour (VX::redLamp);
        g.strokePath (redArc, juce::PathStrokeType (2.4f));
    }

    // labels
    g.setColour (VX::ink);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("VU", face.toNearestInt().withTrimmedTop (8).removeFromTop (16),
                juce::Justification::centred, false);
    g.setFont (juce::Font (8.5f, juce::Font::plain));
    g.drawText ("-20", face.toNearestInt().removeFromBottom (14).removeFromLeft (44),
                juce::Justification::centred, false);
    g.drawText ("0  +3", face.toNearestInt().removeFromBottom (14).removeFromRight (52),
                juce::Justification::centred, false);

    // needle
    auto tip = ptAt (juce::jlimit (0.0f, 1.0f, needle), radius - 4.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (cx, pivotY, tip.x, tip.y, 2.0f);
    // hub
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (cx - 6.0f, pivotY - 6.0f, 12.0f, 12.0f);
    g.setColour (VX::brass);
    g.fillEllipse (cx - 3.0f, pivotY - 3.0f, 6.0f, 6.0f);

    // glass glare
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (face.withTrimmedBottom (face.getHeight() * 0.55f).reduced (3.0f), 4.0f);
}

//==============================================================================
DrowningInVoxAudioProcessorEditor::DrowningInVoxAudioProcessorEditor (DrowningInVoxAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), vu (p)
{
    setLookAndFeel (&laf);

    setupKnob (input, "input", "INPUT");
    setupKnob (drive, "drive", "DRIVE");
    setupKnob (bite, "bite", "BITE");
    setupKnob (body, "body", "BODY");
    setupKnob (compress, "comp", "COMP");
    setupKnob (gate, "gate", "GATE");
    setupKnob (mix, "mix", "MIX");
    setupKnob (output, "output", "OUTPUT");

    modeBox.addItemList ({ "Smooth", "Warm", "Blown" }, 1);
    modeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff141a1f));
    modeBox.setColour (juce::ComboBox::textColourId, VX::engrave);
    modeBox.setColour (juce::ComboBox::outlineColourId, VX::brass.withAlpha (0.75f));
    modeBox.setColour (juce::ComboBox::arrowColourId, VX::brass);
    addAndMakeVisible (modeBox);
    modeAttach = std::make_unique<ComboAttachment> (processor.apvts, "mode", modeBox);

    modeLabel.setText ("MODE", juce::dontSendNotification);
    modeLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    modeLabel.setJustificationType (juce::Justification::centred);
    modeLabel.setColour (juce::Label::textColourId, VX::engrave);
    addAndMakeVisible (modeLabel);

    autoLevelBtn.setColour (juce::ToggleButton::textColourId, VX::engrave);
    autoLevelBtn.setColour (juce::ToggleButton::tickColourId, VX::brass);
    autoLevelBtn.setColour (juce::ToggleButton::tickDisabledColourId, VX::ink);
    autoLevelBtn.setButtonText ("AUTO LEVEL");
    addAndMakeVisible (autoLevelBtn);
    autoLevelAttach = std::make_unique<ButtonAttachment> (processor.apvts, "autolevel", autoLevelBtn);

    addAndMakeVisible (vu);

    setSize (760, 330);
}

DrowningInVoxAudioProcessorEditor::~DrowningInVoxAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void DrowningInVoxAudioProcessorEditor::setupKnob (Knob& k,
                                                   const juce::String& paramID,
                                                   const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 17);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, VX::engrave);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<SliderAttachment> (processor.apvts, paramID, k.slider);
}

void DrowningInVoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // blued-steel panel
    g.setGradientFill (juce::ColourGradient (VX::panelTop, 0, 0, VX::panelBot, 0, bounds.getBottom(), false));
    g.fillAll();

    // side rack rails with screw holes
    g.setColour (VX::rail);
    g.fillRect (0, 0, 30, getHeight());
    g.fillRect (getWidth() - 30, 0, 30, getHeight());
    auto railScrew = [&g] (float x, float y)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (x - 6, y - 6, 12, 12);
        g.setColour (juce::Colour (0xff9aa1a8));
        g.fillEllipse (x - 4.5f, y - 4.5f, 9, 9);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (x - 3, y, x + 3, y, 1.0f);
    };
    for (float yy : { 18.0f, (float) getHeight() / 2.0f, (float) getHeight() - 18.0f })
    {
        railScrew (15.0f, yy);
        railScrew ((float) getWidth() - 15.0f, yy);
    }

    // brushed grain
    juce::Random rng (670);
    for (int i = 0; i < 70; ++i)
    {
        const int y = 16 + rng.nextInt (getHeight() - 32);
        g.setColour ((i % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f));
        g.drawHorizontalLine (y, 34.0f, (float) getWidth() - 34.0f);
    }

    // engraved branding (left of the VU)
    g.setColour (VX::engrave);
    g.setFont (juce::Font (30.0f, juce::Font::bold));
    g.drawText ("FAIRCHILD", 44, 22, 300, 34, juce::Justification::centredLeft, false);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (VX::brass);
    g.drawText ("MODEL 670  -  DROWNING IN VOX", 46, 56, 360, 18, juce::Justification::centredLeft, false);
    g.setColour (VX::engrave.withAlpha (0.75f));
    g.setFont (juce::Font (10.0f, juce::Font::plain));
    g.drawText ("VARI-MU  LEVELLING  AMPLIFIER", 46, 74, 320, 14, juce::Justification::centredLeft, false);

    // brass bezel around the VU meter
    juce::Rectangle<float> plate (514.0f, 12.0f, 212.0f, 108.0f);
    g.setColour (VX::brassDark);
    g.drawRoundedRectangle (plate, 9.0f, 3.0f);
    g.setColour (VX::brass.withAlpha (0.6f));
    g.drawRoundedRectangle (plate.reduced (1.6f), 8.0f, 1.0f);

    // status jewel lamps under the branding
    auto lamp = [&] (float x, float y, juce::Colour c)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        g.setColour (c);
        g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillEllipse (x - 3.5f, y - 4.5f, 4.5f, 4.5f);
    };
    lamp (430.0f, 40.0f, VX::redLamp);
    lamp (458.0f, 40.0f, VX::brass);
}

void DrowningInVoxAudioProcessorEditor::resized()
{
    // VU meter top-center-right
    vu.setBounds (520, 18, 200, 96);

    auto place = [] (Knob& k, int cx, int cy)
    {
        k.label.setBounds (cx - 42, cy - 50, 84, 18);
        k.slider.setBounds (cx - 42, cy - 36, 84, 100);
    };

    const int y1 = 170;
    const int y2 = 262;
    place (input,  82, y1);
    place (drive, 178, y1);
    place (bite,  274, y1);
    place (body,  370, y1);
    place (compress, 466, y1);
    place (gate,  562, y1);
    place (mix,   658, y1);
    place (output, 658, y2);

    modeLabel.setBounds (74, 238, 120, 16);
    modeBox.setBounds (74, 256, 120, 25);

    autoLevelBtn.setBounds (214, 254, 150, 26);
}
