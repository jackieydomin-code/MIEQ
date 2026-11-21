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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    MiEQ::Settings readSettings() const;
    double getCurrentSampleRate() const { return sampleRateForGui.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int numStages = 4;
    juce::dsp::IIR::Filter<float> filters[2][numStages];

    MiEQ::Settings lastSettings;
    bool coeffsDirty = true;
    double sampleRate = 44100.0;
    std::atomic<double> sampleRateForGui { 44100.0 };

    juce::SmoothedValue<float> outputGain { 1.0f };

    std::atomic<float>* pDrive  = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMS     = nullptr;
    std::atomic<float>* pBypass = nullptr;

    void updateCoefficients (const MiEQ::Settings& s);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiEQAudioProcessor)
};
