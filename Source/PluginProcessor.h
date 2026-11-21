#pragma once

#include "Parameters.h"

class MiEQAudioProcessor : public juce::AudioProcessor
{
public:
    MiEQAudioProcessor();
    ~MiEQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mi EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Los presets se gestionan aparte (no como "programas" del host) para que
    // ningún DAW pueda cambiarlos solo al cargar un proyecto.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::StringArray getPresetNames();
    void applyPreset (int index);

    MiEQ::Settings readSettings() const;
    double getCurrentSampleRate() const { return sampleRateForGui.load(); }

    // Picos de salida desde la última consulta (para el medidor)
    float takePeakL() { return peakL.exchange (0.f); }
    float takePeakR() { return peakR.exchange (0.f); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int numStages = 4;
    juce::dsp::IIR::Filter<float> filters[2][numStages];

    using Lin = juce::SmoothedValue<float>;
    using Mul = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;

    Lin smLowBoost { 0.f }, smLowAtten { 0.f }, smHiBoost { 0.f }, smHiBand { 5.f }, smHiAtten { 0.f };
    Mul smLowFreq { 60.f }, smHiFreq { 8000.f }, smHiAttFreq { 10000.f };
    Lin smDrive { 0.f };
    Lin autoGainSm { 1.f };
    Lin outputGain { 1.f };

    MiEQ::Settings targetSettings;
    bool lastAutoGain = false;
    float adaaPrev[2] { 0.f, 0.f };

    double sampleRate = 44100.0;
    std::atomic<double> sampleRateForGui { 44100.0 };
    std::atomic<float> peakL { 0.f }, peakR { 0.f };

    std::atomic<float>* pDrive    = nullptr;
    std::atomic<float>* pOutput   = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pMS       = nullptr;
    std::atomic<float>* pBypass   = nullptr;

    void updateCoefficients (const MiEQ::Settings& s);
    void setTargets (const MiEQ::Settings& s, bool autoGain, bool jumpToTarget);
    void storePeaks (const juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiEQAudioProcessor)
};
