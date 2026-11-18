#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "EqMath.h"

class DodeligQProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;

    DodeligQProcessor();
    ~DodeligQProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "dodeligQ-1"; }
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

    juce::AudioProcessorValueTreeState apvts;

    // Buffer de analizador: lo escribe el hilo de audio, lo lee la GUI
    juce::AbstractFifo scopeFifo { 16384 };
    std::array<float, 16384> scopeBuffer {};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters();
    void pushScope (const juce::AudioBuffer<float>& buffer);

    struct BandParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
    };

    struct BandState
    {
        bool on = false;
        int type = -1;
        float freq = 0.0f, gain = 0.0f, q = 0.0f;
    };

    std::array<BandParams, dq::numBands> params {};
    std::array<BandState, dq::numBands> state {};
    juce::dsp::IIR::Filter<float> filters[dq::numBands][2];
    double currentSampleRate = 44100.0;
    std::atomic<float>* outGainParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DodeligQProcessor)
};
