#include "VocalDistortionProcessor.h"
#include "VocalDistortionEditor.h"

namespace VoxParam
{
    constexpr auto input = "input";
    constexpr auto drive = "drive";
    constexpr auto bite = "bite";
    constexpr auto body = "body";
    constexpr auto comp = "comp";
    constexpr auto gate = "gate";
    constexpr auto mix = "mix";
    constexpr auto output = "output";
    constexpr auto mode = "mode";
    constexpr auto autolevel = "autolevel";
}

DrowningInVoxAudioProcessor::DrowningInVoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
DrowningInVoxAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::input, 1 }, "Input",
        NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::drive, 1 }, "Drive",
        NormalisableRange<float> (0.0f, 36.0f, 0.1f), 12.0f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::bite, 1 }, "Bite",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::body, 1 }, "Body",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::comp, 1 }, "Compress",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::gate, 1 }, "Gate",
        NormalisableRange<float> (-100.0f, -30.0f, 0.1f), -100.0f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { VoxParam::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -3.0f));

    params.push_back (std::make_unique<AudioParameterChoice>(
        ParameterID { VoxParam::mode, 1 }, "Mode",
        StringArray { "Smooth", "Warm", "Blown" }, 1));

    // Auto Level: when on, applies automatic makeup gain that counteracts the
    // Drive setting so the output loudness stays roughly constant as you push
    // harder -- like a leveling amplifier. Default on.
    params.push_back (std::make_unique<AudioParameterBool>(
        ParameterID { VoxParam::autolevel, 1 }, "Auto Level", true));

    return { params.begin(), params.end() };
}

void DrowningInVoxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto numChannels = (juce::uint32) getTotalNumOutputChannels();

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    for (auto& f : inputHPF)
    {
        f.prepare (spec);
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, 85.0f);
        f.reset();
    }
    for (auto& f : prePresence)
    {
        f.prepare (spec);
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 2800.0f, 0.9f, juce::Decibels::decibelsToGain (1.5f));
        f.reset();
    }
    for (auto& f : postBody)
    {
        f.prepare (spec);
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 220.0f, 0.75f, 1.0f);
        f.reset();
    }
    for (auto& f : postBite)
    {
        f.prepare (spec);
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 3400.0f, 0.8f, 1.0f);
        f.reset();
    }
    for (auto& f : postLPF)
    {
        f.prepare (spec);
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 9000.0f);
        f.reset();
    }
    for (auto& f : outputDC)
    {
        f.prepare (spec);
        f.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, 15.0f);
        f.reset();
    }

    lastMode = -1;
    lastBody = std::numeric_limits<float>::quiet_NaN();
    lastBite = std::numeric_limits<float>::quiet_NaN();
    gateGain.fill (1.0f);
    compEnv.fill (0.0f);

    constexpr double ramp = 0.02;
    inputGain.reset (sampleRate, ramp);
    driveGain.reset (sampleRate, ramp);
    outputGain.reset (sampleRate, ramp);
    mixAmt.reset (sampleRate, ramp);
    biasAmt.reset (sampleRate * 4.0, ramp);
    foldAmt.reset (sampleRate * 4.0, ramp);
    compAmt.reset (sampleRate, ramp);
    autoMakeup.reset (sampleRate, 0.05);
    vuMeter = 0.0f;
    vuLinear.store (0.0f);

    dryBuffer.setSize ((int) numChannels, samplesPerBlock);
}

void DrowningInVoxAudioProcessor::releaseResources()
{
    if (oversampling != nullptr)
        oversampling->reset();
}

bool DrowningInVoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void DrowningInVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const float inputDb = apvts.getRawParameterValue (VoxParam::input)->load();
    const float driveDb = apvts.getRawParameterValue (VoxParam::drive)->load();
    const float bite = apvts.getRawParameterValue (VoxParam::bite)->load();
    const float body = apvts.getRawParameterValue (VoxParam::body)->load();
    const float comp = apvts.getRawParameterValue (VoxParam::comp)->load();
    const float gateDb = apvts.getRawParameterValue (VoxParam::gate)->load();
    const float mix = apvts.getRawParameterValue (VoxParam::mix)->load();
    const float outputDb = apvts.getRawParameterValue (VoxParam::output)->load();
    const int mode = (int) apvts.getRawParameterValue (VoxParam::mode)->load();
    const bool autoLevel = apvts.getRawParameterValue (VoxParam::autolevel)->load() > 0.5f;

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (inputDb));
    driveGain.setTargetValue (juce::Decibels::decibelsToGain (driveDb));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputDb));

    // Auto makeup: the soft clipper compresses peaks as Drive rises, so loudness
    // would otherwise jump. Counter it with gain ~ inverse of the drive, scaled
    // back (0.7) because saturation also adds level. Off => unity (1.0).
    const float makeup = autoLevel
        ? std::pow (juce::Decibels::decibelsToGain (-driveDb), 0.7f)
        : 1.0f;
    autoMakeup.setTargetValue (makeup);
    mixAmt.setTargetValue (mix);
    compAmt.setTargetValue (comp);

    const float modeBias = (mode == 2) ? 0.34f : ((mode == 1) ? 0.14f : 0.04f);
    const float modeFold = (mode == 2) ? 0.38f : ((mode == 1) ? 0.12f : 0.0f);
    biasAmt.setTargetValue (modeBias);
    foldAmt.setTargetValue (modeFold);

    if (mode != lastMode || std::isnan (lastBite) || std::abs (bite - lastBite) >= 0.02f)
    {
        lastMode = mode;
        lastBite = bite;
        const float lpHz = (mode == 2) ? juce::jmap (bite, 0.0f, 1.0f, 5200.0f, 10500.0f)
                                      : juce::jmap (bite, 0.0f, 1.0f, 6500.0f, 13500.0f);
        const float biteDb = juce::jmap (bite, 0.0f, 1.0f, -3.0f, 4.0f);
        for (auto& f : postBite)
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, 3300.0f, 0.85f, juce::Decibels::decibelsToGain (biteDb));
        for (auto& f : postLPF)
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, lpHz);
    }

    if (std::isnan (lastBody) || std::abs (body - lastBody) >= 0.02f)
    {
        lastBody = body;
        const float bodyDb = juce::jmap (body, 0.0f, 1.0f, -5.0f, 5.0f);
        for (auto& f : postBody)
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, 220.0f, 0.75f, juce::Decibels::decibelsToGain (bodyDb));
    }

    dryBuffer.makeCopyOf (buffer, true);

    const float gateThr = juce::Decibels::decibelsToGain (gateDb);
    const float gateRelease = 0.9992f;
    for (int ch = 0; ch < numCh && ch < kNumCh; ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        auto ig = inputGain;
        float g = gateGain[(size_t) ch];

        for (int n = 0; n < numSamples; ++n)
        {
            float s = inputHPF[(size_t) ch].processSample (x[n] * ig.getNextValue());
            const float target = (std::abs (s) > gateThr) ? 1.0f : 0.0f;
            g = (target > g) ? target : g * gateRelease + target * (1.0f - gateRelease);
            x[n] = prePresence[(size_t) ch].processSample (s * g);
        }
        gateGain[(size_t) ch] = g;
    }
    inputGain.skip (numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    auto osBlock = oversampling->processSamplesUp (block);
    const int osSamples = (int) osBlock.getNumSamples();

    for (int ch = 0; ch < (int) osBlock.getNumChannels() && ch < kNumCh; ++ch)
    {
        auto* x = osBlock.getChannelPointer ((size_t) ch);
        auto dg = driveGain;
        auto bv = biasAmt;
        auto fv = foldAmt;
        for (int n = 0; n < osSamples; ++n)
            x[n] = vocalShape (x[n] * dg.getNextValue(), bv.getNextValue(), fv.getNextValue());
    }
    driveGain.skip (osSamples);
    biasAmt.skip (osSamples);
    foldAmt.skip (osSamples);

    oversampling->processSamplesDown (block);

    const float atk = 1.0f - std::exp (-1.0f / (0.006f * (float) currentSampleRate));
    const float rel = 1.0f - std::exp (-1.0f / (0.120f * (float) currentSampleRate));
    for (int ch = 0; ch < numCh && ch < kNumCh; ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        float env = compEnv[(size_t) ch];
        auto cv = compAmt;

        for (int n = 0; n < numSamples; ++n)
        {
            float s = postBody[(size_t) ch].processSample (x[n]);
            s = postBite[(size_t) ch].processSample (s);
            s = postLPF[(size_t) ch].processSample (s);

            const float a = std::abs (s);
            env += (a > env ? atk : rel) * (a - env);
            const float clamp = cv.getNextValue() * juce::jmax (0.0f, env - 0.08f) * 5.0f;
            x[n] = s / (1.0f + clamp);
        }
        compEnv[(size_t) ch] = env;
    }
    compAmt.skip (numSamples);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (juce::jmin (ch, dryBuffer.getNumChannels() - 1));
        auto og = outputGain;
        auto mx = mixAmt;
        auto am = autoMakeup;
        for (int n = 0; n < numSamples; ++n)
        {
            float s = wet[n] * am.getNextValue() * og.getNextValue();
            if (ch < kNumCh)
                s = outputDC[(size_t) ch].processSample (s);
            if (! std::isfinite (s)) s = 0.0f;
            const float m = mx.getNextValue();
            wet[n] = juce::jlimit (-1.5f, 1.5f, s) * m + dry[n] * (1.0f - m);
        }
    }
    outputGain.skip (numSamples);
    mixAmt.skip (numSamples);
    autoMakeup.skip (numSamples);

    // --- VU meter: average the output magnitude across channels, apply VU-style
    //     ballistics (~300 ms), publish to the editor. ---
    {
        double sum = 0.0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto* x = buffer.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
                sum += std::abs (x[n]);
        }
        const float avg = numCh > 0 ? (float) (sum / (numCh * juce::jmax (1, numSamples))) : 0.0f;
        const float coeff = 1.0f - std::exp (-(float) numSamples / (0.30f * (float) currentSampleRate));
        vuMeter += coeff * (avg - vuMeter);
        vuLinear.store (vuMeter, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* DrowningInVoxAudioProcessor::createEditor()
{
    return new DrowningInVoxAudioProcessorEditor (*this);
}

void DrowningInVoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrowningInVoxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrowningInVoxAudioProcessor();
}
