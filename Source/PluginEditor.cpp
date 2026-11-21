#include "PluginEditor.h"

namespace
{
    const juce::Colour colLow    { 0xffff9f43 };  // naranja
    const juce::Colour colHiBst  { 0xff2ee6d6 };  // turquesa
    const juce::Colour colHiCut  { 0xffff5fa2 };  // rosa
    const juce::Colour colOut    { 0xffa4e05a };  // lima

    inline float freqToX (float f)   // 20 Hz .. 20 kHz en escala logarítmica -> 0..1
    {
        return std::log (f / 20.f) / std::log (1000.f);
    }
}

//==============================================================================
void MiEQLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    // Pista y valor
    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2f45));
    g.strokePath (track, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Cuerpo del botón
    const float br = r - 10.f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff565d75), c.x, c.y - br,
                                             juce::Colour (0xff1a1d2b), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f, 1.5f);

    // Aguja
    juce::Path pointer;
    pointer.addRoundedRectangle (-2.f, -br + 3.f, 4.f, br * 0.45f, 2.f);
    g.setColour (accent.brighter (0.4f));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void MiEQLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const bool on = b.getToggleState();
    const auto accent = b.getButtonText() == "BYPASS" ? juce::Colour (0xffff5252) : juce::Colour (0xff4da3ff);

    g.setColour (on ? accent : juce::Colour (0xff252a3d));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (accent.withAlpha (highlighted ? 1.f : 0.6f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);

    g.setColour (on ? juce::Colours::black : juce::Colours::white);
    g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
ResponseCurve::ResponseCurve (MiEQAudioProcessor& p) : processor (p)
{
    rebuildCurve();
    startTimerHz (30);
}

void ResponseCurve::timerCallback()
{
    if (! (processor.readSettings() == lastDrawn) || processor.getCurrentSampleRate() != lastRate)
    {
        rebuildCurve();
        repaint();
    }
}

void ResponseCurve::rebuildCurve()
{
    lastDrawn = processor.readSettings();
    lastRate  = processor.getCurrentSampleRate();

    constexpr int n = 256;
    std::vector<double> freqs ((size_t) n), mags ((size_t) n, 1.0), tmp ((size_t) n);
    const double fMax = juce::jmin (20000.0, lastRate * 0.49);

    for (int i = 0; i < n; ++i)
        freqs[(size_t) i] = 20.0 * std::pow (fMax / 20.0, (double) i / (n - 1));

    const auto c = MiEQ::makeCoeffs (lastDrawn, lastRate);
    for (auto ptr : { c.lowBoost, c.lowCut, c.hiBoost, c.hiCut })
    {
        ptr->getMagnitudeForFrequencyArray (freqs.data(), tmp.data(), (size_t) n, lastRate);
        for (size_t i = 0; i < (size_t) n; ++i)
            mags[i] *= tmp[i];
    }

    // Camino normalizado: x en 0..1 (frecuencia), y = -dB (se escala al pintar)
    curve.clear();
    for (int i = 0; i < n; ++i)
    {
        const float db = juce::jlimit (-20.f, 20.f, (float) juce::Decibels::gainToDecibels (mags[(size_t) i]));
        const float x = freqToX ((float) freqs[(size_t) i]);
        if (i == 0) curve.startNewSubPath (x, -db);
        else        curve.lineTo (x, -db);
    }
}

void ResponseCurve::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0e19));
    g.fillRoundedRectangle (b, 10.f);

    const auto area = b.reduced (12.f, 14.f);
    const float centreY = area.getCentreY();
    const float dbToY = area.getHeight() / 40.f;

    g.setFont (juce::FontOptions (11.f));

    // Rejilla de frecuencia
    const std::pair<float, const char*> marks[] = { { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" },
                                                    { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" } };
    for (auto& m : marks)
    {
        const float x = area.getX() + area.getWidth() * freqToX (m.first);
        g.setColour (juce::Colour (0xff1f2438));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (juce::Colours::grey);
        g.drawText (m.second, (int) x - 20, (int) area.getBottom() - 2, 40, 14, juce::Justification::centred);
    }

    // Rejilla de dB
    for (int db : { -12, -6, 0, 6, 12 })
    {
        const float y = centreY - (float) db * dbToY;
        g.setColour (db == 0 ? juce::Colour (0xff3a4160) : juce::Colour (0xff1f2438));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (juce::Colours::grey);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), (int) area.getX() - 10, (int) y - 7, 28, 14,
                    juce::Justification::centredLeft);
    }

    // Curva
    juce::Path p (curve);
    p.applyTransform (juce::AffineTransform::scale (area.getWidth(), dbToY).translated (area.getX(), centreY));

    juce::Path fill (p);
    fill.lineTo (area.getRight(), centreY);
    fill.lineTo (area.getX(), centreY);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colHiBst.withAlpha (0.30f), 0.f, area.getY(),
                                             colLow.withAlpha (0.10f), 0.f, area.getBottom(), false));
    g.fillPath (fill);

    g.setColour (juce::Colours::white);
    g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
MiEQAudioProcessorEditor::MiEQAudioProcessorEditor (MiEQAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), curve (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (curve);

    setupKnob (lowFreq,   "Frecuencia", MiEQ::ID::lowFreq,   colLow,   true);
    setupKnob (lowBoost,  "Boost",      MiEQ::ID::lowBoost,  colLow,   false);
    setupKnob (lowAtten,  "Atten",      MiEQ::ID::lowAtten,  colLow,   false);

    setupKnob (hiFreq,    "Frecuencia", MiEQ::ID::hiFreq,    colHiBst, true);
    setupKnob (hiBoost,   "Boost",      MiEQ::ID::hiBoost,   colHiBst, false);
    setupKnob (hiBand,    "Ancho",      MiEQ::ID::hiBand,    colHiBst, false);

    setupKnob (hiAttFreq, "Frecuencia", MiEQ::ID::hiAttFreq, colHiCut, true);
    setupKnob (hiAtten,   "Atten",      MiEQ::ID::hiAtten,   colHiCut, false);

    setupKnob (drive,     "Drive",      MiEQ::ID::drive,     colOut,   false);
    setupKnob (output,    "Salida",     MiEQ::ID::output,    colOut,   false);
    output.slider.setTextValueSuffix (" dB");

    panels[0] = { "LOW  -  GRAVES",         colLow,   {} };
    panels[1] = { "HIGH BOOST  -  AGUDOS +", colHiBst, {} };
    panels[2] = { "HIGH CUT  -  AGUDOS -",   colHiCut, {} };
    panels[3] = { "OUTPUT",                 colOut,   {} };

    addAndMakeVisible (msButton);
    addAndMakeVisible (bypassButton);
    msAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, MiEQ::ID::ms, msButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, MiEQ::ID::bypass, bypassButton);

    setSize (1000, 440);
}

MiEQAudioProcessorEditor::~MiEQAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MiEQAudioProcessorEditor::setupKnob (Knob& k, const juce::String& title, const juce::String& paramID,
                                          juce::Colour colour, bool isChoice)
{
    k.title = title;
    auto& s = k.slider;

    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setColour (juce::Slider::rotarySliderFillColourId, colour);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramID, s);

    if (auto* param = processor.apvts.getParameter (paramID))
    {
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        if (isChoice)
        {
            s.setTextBoxIsEditable (false);
            s.textFromValueFunction = [param] (double v) { return param->getText (param->convertTo0to1 ((float) v), 0); };
            s.updateText();
        }
    }
}

void MiEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0f1220), 0.f, 0.f,
                                             juce::Colour (0xff1c2136), 0.f, (float) getHeight(), false));
    g.fillAll();

    // Cabecera
    auto header = getLocalBounds().reduced (16).removeFromTop (48);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (32.f, juce::Font::bold));
    g.drawText ("MI EQ", header.removeFromLeft (110), juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (13.f));
    g.drawText ("Ecualizador pasivo estilo Pultec  -  boost y atten a la vez", header.withTrimmedLeft (10),
                juce::Justification::centredLeft);

    // Paneles
    for (auto& panel : panels)
    {
        const auto a = panel.area.toFloat();
        g.setColour (juce::Colour (0xff1b2033));
        g.fillRoundedRectangle (a, 10.f);
        g.setColour (panel.colour.withAlpha (0.35f));
        g.drawRoundedRectangle (a, 10.f, 1.2f);

        g.setColour (panel.colour);
        g.setFont (juce::FontOptions (14.f, juce::Font::bold));
        g.drawText (panel.name, panel.area.withHeight (30).withTrimmedLeft (14), juce::Justification::centredLeft);
    }

    // Nombres de cada mando
    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::FontOptions (13.f));
    for (auto* k : { &lowFreq, &lowBoost, &lowAtten, &hiFreq, &hiBoost, &hiBand,
                     &hiAttFreq, &hiAtten, &drive, &output })
        g.drawText (k->title, k->area.withHeight (20), juce::Justification::centred);
}

void MiEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (48);
    bypassButton.setBounds (header.removeFromRight (110).reduced (0, 10));
    header.removeFromRight (8);
    msButton.setBounds (header.removeFromRight (120).reduced (0, 10));

    area.removeFromTop (6);
    curve.setBounds (area.removeFromTop (170));
    area.removeFromTop (12);

    const int gap = 10;
    const int unit = (area.getWidth() - 3 * gap) / 10;
    const int weights[4] = { 3, 3, 2, 2 };
    for (int i = 0; i < 4; ++i)
    {
        panels[(size_t) i].area = (i == 3) ? area : area.removeFromLeft (unit * weights[i]);
        area.removeFromLeft (gap);
    }

    auto layoutKnobs = [] (juce::Rectangle<int> panelArea, std::initializer_list<Knob*> knobs)
    {
        auto inner = panelArea.withTrimmedTop (32).reduced (6, 2);
        const int w = inner.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            k->area = inner.removeFromLeft (w);
            auto sliderArea = k->area;
            sliderArea.removeFromTop (20);
            k->slider.setBounds (sliderArea);
        }
    };

    layoutKnobs (panels[0].area, { &lowFreq, &lowBoost, &lowAtten });
    layoutKnobs (panels[1].area, { &hiFreq, &hiBoost, &hiBand });
    layoutKnobs (panels[2].area, { &hiAttFreq, &hiAtten });
    layoutKnobs (panels[3].area, { &drive, &output });
}
