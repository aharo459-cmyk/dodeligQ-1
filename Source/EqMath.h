#pragma once
#include <juce_dsp/juce_dsp.h>

namespace dq
{
constexpr int numBands = 8;

// Orden de los tipos = orden del ComboBox y del parámetro "type"
enum class FilterType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };

// Los tipos 0..2 usan ganancia; los demás no
inline bool typeHasGain (int type) { return type <= 2; }

inline juce::dsp::IIR::Coefficients<float>::Ptr makeCoefs (FilterType type,
                                                          double sampleRate,
                                                          float freq,
                                                          float gainDb,
                                                          float q)
{
    using C = juce::dsp::IIR::Coefficients<float>;
    freq = juce::jlimit (10.0f, (float) sampleRate * 0.49f, freq);
    q = juce::jmax (0.1f, q);
    const float gainLin = juce::Decibels::decibelsToGain (gainDb);

    switch (type)
    {
        case FilterType::Bell:      return C::makePeakFilter (sampleRate, freq, q, gainLin);
        case FilterType::LowShelf:  return C::makeLowShelf (sampleRate, freq, q, gainLin);
        case FilterType::HighShelf: return C::makeHighShelf (sampleRate, freq, q, gainLin);
        case FilterType::LowCut:    return C::makeHighPass (sampleRate, freq, q);
        case FilterType::HighCut:   return C::makeLowPass (sampleRate, freq, q);
        case FilterType::Notch:     return C::makeNotch (sampleRate, freq, q);
    }
    return C::makePeakFilter (sampleRate, freq, q, 1.0f);
}
} // namespace dq
