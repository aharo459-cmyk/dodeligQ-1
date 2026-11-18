#pragma once
#include <array>
#include <memory>
#include "PluginProcessor.h"

class DodeligQEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit DodeligQEditor (DodeligQProcessor&);
    ~DodeligQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void selectBand (int index);
    void readScope();
    void computeSpectrum();

    float bandParam (int band, const char* suffix) const;
    void setBandParam (int band, const char* suffix, float value);
    juce::Rectangle<float> plotArea() const;
    float xFromFreq (float f) const;
    float freqFromX (float x) const;
    float yFromGain (float g) const;
    float gainFromY (float y) const;
    float yFromSpec (float db) const;
    float nodeY (int band) const;
    int nodeAt (juce::Point<float> pos) const;

    DodeligQProcessor& proc;

    juce::dsp::FFT fft { DodeligQProcessor::fftOrder };
    std::array<float, DodeligQProcessor::fftSize * 2> fftData {};
    std::array<float, DodeligQProcessor::fftSize> ring {};
    std::array<float, DodeligQProcessor::fftSize / 2> spectrum {};
    int ringPos = 0;

    int selected = 0;
    int dragging = -1;

    juce::TextButton bandButtons[dq::numBands];
    juce::ComboBox typeBox;
    juce::ToggleButton onButton;
    juce::Slider freqSlider, gainSlider, qSlider, outSlider;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ComboAtt> typeAtt;
    std::unique_ptr<ButtonAtt> onAtt;
    std::unique_ptr<SliderAtt> freqAtt, gainAtt, qAtt, outAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DodeligQEditor)
};
