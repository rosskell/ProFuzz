#pragma once

#include <JuceHeader.h>
#include "VocalDistortionProcessor.h"

//==============================================================================
// Fairchild-style look: dark chicken-head pointer knobs on a blued-steel panel.
class VoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
// A real, working VU meter: classic arc dial with a needle driven by the
// processor's output level. Repaints on a 30 Hz timer with needle inertia so it
// swings like a real moving-coil movement.
class VUMeter : public juce::Component,
                private juce::Timer
{
public:
    explicit VUMeter (DrowningInVoxAudioProcessor& p);
    ~VUMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    DrowningInVoxAudioProcessor& processor;
    float needle = 0.0f;   // smoothed 0..1 deflection actually drawn
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMeter)
};

//==============================================================================
class DrowningInVoxAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DrowningInVoxAudioProcessorEditor (DrowningInVoxAudioProcessor&);
    ~DrowningInVoxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attach;
    };

    void setupKnob (Knob&, const juce::String& paramID, const juce::String& label);

    DrowningInVoxAudioProcessor& processor;
    VoxLookAndFeel laf;

    Knob input, drive, bite, body, compress, gate, mix, output;

    juce::ComboBox modeBox;
    juce::Label modeLabel;
    std::unique_ptr<ComboAttachment> modeAttach;

    juce::ToggleButton autoLevelBtn;
    juce::Label autoLevelLabel;
    std::unique_ptr<ButtonAttachment> autoLevelAttach;

    VUMeter vu;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrowningInVoxAudioProcessorEditor)
};
