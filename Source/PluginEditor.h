#pragma once

#include "PluginProcessor.h"

//==============================================================================
class MiEQLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
// Gráfica en vivo de la curva de respuesta (se calcula con los mismos filtros que el audio)
class ResponseCurve : public juce::Component, private juce::Timer
{
public:
    explicit ResponseCurve (MiEQAudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    MiEQAudioProcessor& processor;
    MiEQ::Settings lastDrawn;
    double lastRate = 0.0;
    juce::Path curve;

    void rebuildCurve();
};

//==============================================================================
// Medidor de nivel de salida (L/R)
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (MiEQAudioProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    MiEQAudioProcessor& processor;
    float dbL = -80.f, dbR = -80.f;
};

//==============================================================================
class MiEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MiEQAudioProcessorEditor (MiEQAudioProcessor&);
    ~MiEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::String title;
        juce::Rectangle<int> area;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Panel
    {
        juce::String name;
        juce::Colour colour;
        juce::Rectangle<int> area;
    };

    MiEQAudioProcessor& processor;
    MiEQLookAndFeel lookAndFeel;
    ResponseCurve curve;
    LevelMeter meter;

    Knob lowFreq, lowBoost, lowAtten;
    Knob hiFreq, hiBoost, hiBand;
    Knob hiAttFreq, hiAtten;
    Knob drive, output;
    std::array<Panel, 4> panels;

    juce::ComboBox presetBox;
    juce::ToggleButton autoGainButton { "AUTO VOL" }, msButton { "MID / SIDE" }, bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment, msAttachment, bypassAttachment;

    void setupKnob (Knob&, const juce::String& title, const juce::String& paramID,
                    juce::Colour colour, bool isChoice);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiEQAudioProcessorEditor)
};
