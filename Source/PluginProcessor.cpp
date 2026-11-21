#include "PluginProcessor.h"
#include "PluginEditor.h"

MiEQAudioProcessor::MiEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MiEQ_STATE", MiEQ::createLayout())
{
    pDrive  = apvts.getRawParameterValue (MiEQ::ID::drive);
    pOutput = apvts.getRawParameterValue (MiEQ::ID::output);
    pMS     = apvts.getRawParameterValue (MiEQ::ID::ms);
    pBypass = apvts.getRawParameterValue (MiEQ::ID::bypass);
}

bool MiEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

MiEQ::Settings MiEQAudioProcessor::readSettings() const
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    MiEQ::Settings s;
    s.lowFreq   = MiEQ::lowFreqs   [juce::jlimit (0, 3, (int) std::round (get (MiEQ::ID::lowFreq)))];
    s.lowBoost  = get (MiEQ::ID::lowBoost);
    s.lowAtten  = get (MiEQ::ID::lowAtten);
    s.hiFreq    = MiEQ::hiFreqs    [juce::jlimit (0, 6, (int) std::round (get (MiEQ::ID::hiFreq)))];
    s.hiBoost   = get (MiEQ::ID::hiBoost);
    s.hiBand    = get (MiEQ::ID::hiBand);
    s.hiAttFreq = MiEQ::hiAttFreqs [juce::jlimit (0, 2, (int) std::round (get (MiEQ::ID::hiAttFreq)))];
    s.hiAtten   = get (MiEQ::ID::hiAtten);
    return s;
}

void MiEQAudioProcessor::updateCoefficients (const MiEQ::Settings& s)
{
    auto c = MiEQ::makeCoeffs (s, sampleRate);

    for (auto& chain : filters)
    {
        chain[0].coefficients = c.lowBoost;
        chain[1].coefficients = c.lowCut;
        chain[2].coefficients = c.hiBoost;
        chain[3].coefficients = c.hiCut;
    }

    lastSettings = s;
    coeffsDirty = false;
}

void MiEQAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    sampleRateForGui.store (newSampleRate);

    updateCoefficients (readSettings());

    for (auto& chain : filters)
        for (auto& f : chain)
            f.reset();

    outputGain.reset (newSampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
}

void MiEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (pBypass->load() > 0.5f)
        return; // el audio pasa sin tocar

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    // Recalcular filtros solo si algún control ha cambiado
    const auto settings = readSettings();
    if (coeffsDirty || ! (settings == lastSettings))
        updateCoefficients (settings);

    const bool useMS = pMS->load() > 0.5f && numChannels == 2;

    if (useMS)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]);
            l[i] = m;
            r[i] = s;
        }
    }

    // --- Ecualización ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];
            for (int st = 0; st < numStages; ++st)
                x = filters[ch][st].processSample (x);
            data[i] = x;
        }
    }

    // --- Drive (saturación suave) ---
    const float drive = pDrive->load();
    if (drive > 0.01f)
    {
        const float g = 1.0f + drive * 0.35f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::tanh (g * data[i]) / g;
        }
    }

    if (useMS)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = l[i];
            const float s = r[i];
            l[i] = m + s;
            r[i] = m - s;
        }
    }

    // --- Salida (suavizada para evitar chasquidos) ---
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

juce::AudioProcessorEditor* MiEQAudioProcessor::createEditor()
{
    return new MiEQAudioProcessorEditor (*this);
}

void MiEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MiEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MiEQAudioProcessor();
}
