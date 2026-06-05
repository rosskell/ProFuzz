#pragma once

#include <JuceHeader.h>

class DrowningInVoxAudioProcessor : public juce::AudioProcessor
{
public:
    DrowningInVoxAudioProcessor();
    ~DrowningInVoxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Output level for the VU meter (linear RMS, written on the audio thread,
    // read by the editor's timer). Atomic = no lock, no tearing.
    std::atomic<float> vuLinear { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static inline float vocalShape (float x, float bias, float fold) noexcept
    {
        const float asym = (x >= 0.0f) ? 1.0f + bias * 0.25f : 1.0f - bias * 0.35f;
        const float soft = std::tanh (x * asym);
        const float folded = std::sin (soft * juce::MathConstants<float>::halfPi);
        return soft * (1.0f - fold) + folded * fold;
    }

    double currentSampleRate = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> dryBuffer;

    static constexpr int kNumCh = 2;

    std::array<juce::dsp::IIR::Filter<float>, kNumCh> inputHPF;
    std::array<juce::dsp::IIR::Filter<float>, kNumCh> prePresence;
    std::array<juce::dsp::IIR::Filter<float>, kNumCh> postBody;
    std::array<juce::dsp::IIR::Filter<float>, kNumCh> postBite;
    std::array<juce::dsp::IIR::Filter<float>, kNumCh> postLPF;
    std::array<juce::dsp::IIR::Filter<float>, kNumCh> outputDC;

    std::array<float, kNumCh> gateGain { 1.0f, 1.0f };
    std::array<float, kNumCh> compEnv { 0.0f, 0.0f };

    int lastMode = -1;
    float lastBody = std::numeric_limits<float>::quiet_NaN();
    float lastBite = std::numeric_limits<float>::quiet_NaN();

    // Auto-level (auto makeup gain): persistent slow RMS envelopes for the dry
    // (pre-distortion) and wet (post) signals. Per-block RMS is too noisy and
    // causes pumping, so we integrate across blocks (~400 ms) and take the ratio
    // from the smoothed envelopes. Mean-square domain; sqrt at use.
    double dryEnvSq = 0.0;
    double wetEnvSq = 0.0;
    juce::SmoothedValue<float> autoMakeup;
    float vuMeter = 0.0f; // VU ballistics state (audio thread)

    juce::SmoothedValue<float> inputGain, driveGain, outputGain, mixAmt, biasAmt, foldAmt, compAmt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrowningInVoxAudioProcessor)
};
