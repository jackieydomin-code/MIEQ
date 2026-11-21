#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Mi EQ v1.1 - EQ pasivo estilo "Reference 1956" (curvas tipo Pultec):
//   LOW:        shelf de graves con BOOST y ATTEN simultáneos (el "low-end trick")
//   HIGH BOOST: campana de agudos con ancho de banda variable
//   HIGH CUT:   shelf de atenuación de agudos
//   + Drive (saturación suave tipo válvula), Output, Auto Volume, Mid/Side y Bypass.
namespace MiEQ
{
namespace ID
{
    static const juce::String lowFreq   { "lowFreq" };
    static const juce::String lowBoost  { "lowBoost" };
    static const juce::String lowAtten  { "lowAtten" };
    static const juce::String hiFreq    { "hiFreq" };
    static const juce::String hiBoost   { "hiBoost" };
    static const juce::String hiBand    { "hiBand" };
    static const juce::String hiAttFreq { "hiAttFreq" };
    static const juce::String hiAtten   { "hiAtten" };
    static const juce::String drive     { "drive" };
    static const juce::String output    { "output" };
    static const juce::String autoGain  { "autoGain" };
    static const juce::String ms        { "ms" };
    static const juce::String bypass    { "bypass" };
}

inline const juce::StringArray lowFreqNames   { "20 Hz", "30 Hz", "60 Hz", "100 Hz" };
inline const juce::StringArray hiFreqNames    { "3 kHz", "4 kHz", "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" };
inline const juce::StringArray hiAttFreqNames { "5 kHz", "10 kHz", "20 kHz" };

inline constexpr float lowFreqs[]   { 20.f, 30.f, 60.f, 100.f };
inline constexpr float hiFreqs[]    { 3000.f, 4000.f, 5000.f, 8000.f, 10000.f, 12000.f, 16000.f };
inline constexpr float hiAttFreqs[] { 5000.f, 10000.f, 20000.f };

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto knob = [] (const String& id, const String& name)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.f, 10.f, 0.1f), 0.f);
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::lowFreq, 1 }, "Low Freq", lowFreqNames, 2));
    layout.add (knob (ID::lowBoost, "Low Boost"));
    layout.add (knob (ID::lowAtten, "Low Atten"));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::hiFreq, 1 }, "High Boost Freq", hiFreqNames, 3));
    layout.add (knob (ID::hiBoost, "High Boost"));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::hiBand, 1 }, "High Bandwidth",
                                                       NormalisableRange<float> (0.f, 10.f, 0.1f), 5.f));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::hiAttFreq, 1 }, "High Cut Freq", hiAttFreqNames, 1));
    layout.add (knob (ID::hiAtten, "High Cut"));

    layout.add (knob (ID::drive, "Drive"));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::output, 1 }, "Output",
                                                       NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ID::autoGain, 1 }, "Auto Volume", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ID::ms, 1 }, "Mid/Side", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ID::bypass, 1 }, "Bypass", false));

    return layout;
}

struct Settings
{
    float lowFreq = 60.f, lowBoost = 0.f, lowAtten = 0.f;
    float hiFreq = 8000.f, hiBoost = 0.f, hiBand = 5.f;
    float hiAttFreq = 10000.f, hiAtten = 0.f;

    bool operator== (const Settings&) const = default;
};

struct Coeffs
{
    using C = juce::dsp::IIR::Coefficients<float>;
    C::Ptr lowBoost, lowCut, hiBoost, hiCut;
};

// Misma función para el audio y para la curva del editor, así el dibujo siempre coincide con el sonido.
inline Coeffs makeCoeffs (const Settings& s, double sampleRate)
{
    using C = Coeffs::C;

    const auto maxFreq = (float) (sampleRate * 0.45);
    auto clampF = [maxFreq] (float f) { return juce::jmin (f, maxFreq); };
    auto gainFor = [] (float amount, float maxDb)
    {
        return juce::Decibels::decibelsToGain (amount / 10.f * maxDb);
    };

    Coeffs c;
    c.lowBoost = C::makeLowShelf (sampleRate, clampF (s.lowFreq), 0.65f, gainFor (s.lowBoost, 14.f));
    c.lowCut   = C::makeLowShelf (sampleRate, clampF (s.lowFreq), 0.40f, gainFor (s.lowAtten, -16.f));

    // Bandwidth: 0 = estrecho (Q alto) ... 10 = ancho (Q bajo)
    const float q = juce::jmap (s.hiBand / 10.f, 2.0f, 0.35f);
    c.hiBoost = C::makePeakFilter (sampleRate, clampF (s.hiFreq), q, gainFor (s.hiBoost, 16.f));
    c.hiCut   = C::makeHighShelf (sampleRate, clampF (s.hiAttFreq), 0.50f, gainFor (s.hiAtten, -16.f));
    return c;
}

// Compensación de volumen: mide cuánto sube o baja el nivel medio de la curva
// (todas las zonas de frecuencia pesan igual en escala logarítmica, como ruido rosa)
// y devuelve la ganancia que lo devuelve a su nivel original.
inline float computeAutoGain (const Settings& s, double sampleRate)
{
    constexpr int n = 64;
    std::array<double, n> freqs {}, mags {}, tmp {};
    const double fMax = juce::jmin (20000.0, sampleRate * 0.49);

    for (int i = 0; i < n; ++i)
    {
        freqs[(size_t) i] = 20.0 * std::pow (fMax / 20.0, (double) i / (n - 1));
        mags[(size_t) i] = 1.0;
    }

    const auto c = makeCoeffs (s, sampleRate);
    for (auto ptr : { c.lowBoost, c.lowCut, c.hiBoost, c.hiCut })
    {
        ptr->getMagnitudeForFrequencyArray (freqs.data(), tmp.data(), (size_t) n, sampleRate);
        for (size_t i = 0; i < (size_t) n; ++i)
            mags[i] *= tmp[i];
    }

    double sum = 0.0;
    for (auto m : mags)
        sum += m * m;

    const double rms = std::sqrt (sum / n);
    return juce::jlimit (0.25f, 4.0f, (float) (1.0 / juce::jmax (rms, 1.0e-6)));
}
} // namespace MiEQ
