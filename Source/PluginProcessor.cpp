#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Orden: lowFreq(índice), lowBoost, lowAtten, hiFreq(índice), hiBoost, hiBand,
    //        hiAttFreq(índice), hiAtten, drive, output(dB)
    struct Preset { const char* name; float v[10]; };

    const Preset presets[] =
    {
        { "Init (plano)",        { 2, 0.f, 0.f,  3, 0.f, 5.f,  1, 0.f,  0.f, 0.f } },
        { "Voz con brillo",      { 3, 0.f, 1.5f, 4, 3.f, 6.f,  1, 0.f,  0.f, 0.f } },
        { "Bombo grueso",        { 2, 4.f, 2.f,  3, 0.f, 5.f,  1, 0.f,  0.f, 0.f } },
        { "Bajo profundo",       { 1, 4.f, 2.5f, 3, 0.f, 5.f,  1, 1.f,  0.f, 0.f } },
        { "Aire en la mezcla",   { 2, 0.f, 0.f,  5, 2.5f, 7.f, 1, 0.f,  0.f, 0.f } },
        { "Suavizar agudos",     { 2, 0.f, 0.f,  3, 0.f, 5.f,  1, 3.f,  0.f, 0.f } },
        { "Calidez de valvula",  { 2, 2.f, 0.f,  3, 0.f, 5.f,  1, 1.5f, 3.f, 0.f } },
        { "Master suave",        { 2, 1.5f, 0.f, 5, 1.5f, 8.f, 1, 0.f,  1.f, 0.f } },
    };

    inline float lnCosh (float x)
    {
        x = std::abs (x);
        return x + std::log1p (std::exp (-2.f * x)) - 0.6931472f;
    }
}

MiEQAudioProcessor::MiEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MiEQ_STATE", MiEQ::createLayout())
{
    pDrive    = apvts.getRawParameterValue (MiEQ::ID::drive);
    pOutput   = apvts.getRawParameterValue (MiEQ::ID::output);
    pAutoGain = apvts.getRawParameterValue (MiEQ::ID::autoGain);
    pMS       = apvts.getRawParameterValue (MiEQ::ID::ms);
    pBypass   = apvts.getRawParameterValue (MiEQ::ID::bypass);
}

bool MiEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

juce::StringArray MiEQAudioProcessor::getPresetNames()
{
    juce::StringArray names;
    for (auto& p : presets)
        names.add (p.name);
    return names;
}

void MiEQAudioProcessor::applyPreset (int index)
{
    if (index < 0 || index >= (int) std::size (presets))
        return;

    const juce::String ids[10] = { MiEQ::ID::lowFreq, MiEQ::ID::lowBoost, MiEQ::ID::lowAtten,
                                   MiEQ::ID::hiFreq,  MiEQ::ID::hiBoost,  MiEQ::ID::hiBand,
                                   MiEQ::ID::hiAttFreq, MiEQ::ID::hiAtten,
                                   MiEQ::ID::drive, MiEQ::ID::output };

    for (int i = 0; i < 10; ++i)
        if (auto* p = apvts.getParameter (ids[i]))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (presets[index].v[i]));
            p->endChangeGesture();
        }
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
}

void MiEQAudioProcessor::setTargets (const MiEQ::Settings& t, bool autoGain, bool jumpToTarget)
{
    auto set = [jumpToTarget] (auto& smoother, float value)
    {
        if (jumpToTarget) smoother.setCurrentAndTargetValue (value);
        else              smoother.setTargetValue (value);
    };

    set (smLowFreq,   t.lowFreq);
    set (smLowBoost,  t.lowBoost);
    set (smLowAtten,  t.lowAtten);
    set (smHiFreq,    t.hiFreq);
    set (smHiBoost,   t.hiBoost);
    set (smHiBand,    t.hiBand);
    set (smHiAttFreq, t.hiAttFreq);
    set (smHiAtten,   t.hiAtten);
    set (autoGainSm,  autoGain ? MiEQ::computeAutoGain (t, sampleRate) : 1.f);

    targetSettings = t;
    lastAutoGain = autoGain;
}

void MiEQAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    sampleRateForGui.store (newSampleRate);

    // Todos los cambios de mando se suavizan durante 30 ms para evitar chasquidos
    const double ramp = 0.03;
    for (auto* s : { &smLowBoost, &smLowAtten, &smHiBoost, &smHiBand, &smHiAtten, &smDrive, &autoGainSm })
        s->reset (newSampleRate, ramp);
    for (auto* s : { &smLowFreq, &smHiFreq, &smHiAttFreq })
        s->reset (newSampleRate, ramp);
    outputGain.reset (newSampleRate, 0.02);

    setTargets (readSettings(), pAutoGain->load() > 0.5f, true);
    smDrive.setCurrentAndTargetValue (pDrive->load());
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    updateCoefficients (targetSettings);

    for (auto& chain : filters)
        for (auto& f : chain)
            f.reset();

    adaaPrev[0] = adaaPrev[1] = 0.f;
}

void MiEQAudioProcessor::storePeaks (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 1 || n == 0)
        return;

    const float pl = buffer.getMagnitude (0, 0, n);
    const float pr = buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, n) : pl;

    if (pl > peakL.load()) peakL.store (pl);
    if (pr > peakR.load()) peakR.store (pr);
}

void MiEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (pBypass->load() > 0.5f)
    {
        storePeaks (buffer);
        return; // el audio pasa sin tocar
    }

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    // Nuevos valores de los mandos -> los suavizadores se encargan de llegar a ellos sin saltos
    const auto target = readSettings();
    const bool autoGainOn = pAutoGain->load() > 0.5f;
    if (! (target == targetSettings) || autoGainOn != lastAutoGain)
        setTargets (target, autoGainOn, false);

    smDrive.setTargetValue (pDrive->load());

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

    auto anyMoving = [this]
    {
        return smLowBoost.isSmoothing() || smLowAtten.isSmoothing() || smHiBoost.isSmoothing()
            || smHiBand.isSmoothing()   || smHiAtten.isSmoothing()   || smLowFreq.isSmoothing()
            || smHiFreq.isSmoothing()   || smHiAttFreq.isSmoothing();
    };

    // --- Ecualización + Drive, en trozos de 32 muestras ---
    constexpr int chunk = 32;
    for (int start = 0; start < numSamples; start += chunk)
    {
        const int n = juce::jmin (chunk, numSamples - start);

        if (anyMoving())
        {
            MiEQ::Settings cur;
            cur.lowFreq   = smLowFreq.skip (n);
            cur.lowBoost  = smLowBoost.skip (n);
            cur.lowAtten  = smLowAtten.skip (n);
            cur.hiFreq    = smHiFreq.skip (n);
            cur.hiBoost   = smHiBoost.skip (n);
            cur.hiBand    = smHiBand.skip (n);
            cur.hiAttFreq = smHiAttFreq.skip (n);
            cur.hiAtten   = smHiAtten.skip (n);
            updateCoefficients (cur);
        }

        const float g = 1.0f + smDrive.skip (n) * 0.35f;
        const bool driveOn = g > 1.0005f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch) + start;
            float prev = adaaPrev[ch];

            for (int i = 0; i < n; ++i)
            {
                float x = data[i];
                for (int st = 0; st < numStages; ++st)
                    x = filters[ch][st].processSample (x);

                if (driveOn)
                {
                    // tanh con anti-aliasing (ADAA de 1er orden): saturación suave sin "ruido digital" en agudos
                    const float d = x - prev;
                    const float avg = std::abs (d) < 1.0e-6f
                                          ? std::tanh (g * 0.5f * (x + prev))
                                          : (lnCosh (g * x) - lnCosh (g * prev)) / (g * d);
                    prev = x;
                    data[i] = avg / g;
                }
                else
                {
                    prev = x;
                    data[i] = x;
                }
            }

            adaaPrev[ch] = prev;
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

    // --- Salida: volumen + compensación automática (suavizados) ---
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = outputGain.getNextValue() * autoGainSm.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }

    storePeaks (buffer);
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
