#include "PluginProcessor.h"
#include "PluginEditor.h"

DodeligQProcessor::DodeligQProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < dq::numBands; ++i)
    {
        const auto n = juce::String (i + 1);
        params[i].on   = apvts.getRawParameterValue ("b" + n + "_on");
        params[i].type = apvts.getRawParameterValue ("b" + n + "_type");
        params[i].freq = apvts.getRawParameterValue ("b" + n + "_freq");
        params[i].gain = apvts.getRawParameterValue ("b" + n + "_gain");
        params[i].q    = apvts.getRawParameterValue ("b" + n + "_q");
    }
    outGainParam = apvts.getRawParameterValue ("outGain");
}

juce::AudioProcessorValueTreeState::ParameterLayout DodeligQProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    static const float defaultFreqs[dq::numBands] { 60.f, 150.f, 400.f, 1000.f, 2500.f, 5000.f, 10000.f, 16000.f };
    const juce::StringArray types { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };

    for (int i = 0; i < dq::numBands; ++i)
    {
        const auto n = juce::String (i + 1);
        const auto name = "Band " + n;

        p.push_back (std::make_unique<juce::AudioParameterBool> ("b" + n + "_on", name + " On", false));
        p.push_back (std::make_unique<juce::AudioParameterChoice> ("b" + n + "_type", name + " Type", types, 0));
        p.push_back (std::make_unique<juce::AudioParameterFloat> ("b" + n + "_freq", name + " Freq",
                        juce::NormalisableRange<float> (10.f, 30000.f, 0.f, 0.25f), defaultFreqs[i]));
        p.push_back (std::make_unique<juce::AudioParameterFloat> ("b" + n + "_gain", name + " Gain",
                        juce::NormalisableRange<float> (-30.f, 30.f, 0.01f), 0.f));
        p.push_back (std::make_unique<juce::AudioParameterFloat> ("b" + n + "_q", name + " Q",
                        juce::NormalisableRange<float> (0.1f, 18.f, 0.f, 0.5f), 1.0f));
    }

    p.push_back (std::make_unique<juce::AudioParameterFloat> ("outGain", "Output Gain",
                    juce::NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f));

    return { p.begin(), p.end() };
}

bool DodeligQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void DodeligQProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    for (auto& band : filters)
        for (auto& f : band)
            f.reset();
    for (auto& s : state)
        s = BandState {};   // fuerza recálculo de coeficientes
    scopeFifo.reset();
}

void DodeligQProcessor::updateFilters()
{
    for (int b = 0; b < dq::numBands; ++b)
    {
        BandState now;
        now.on   = params[b].on->load() > 0.5f;
        now.type = juce::roundToInt (params[b].type->load());
        now.freq = params[b].freq->load();
        now.gain = params[b].gain->load();
        now.q    = params[b].q->load();

        const auto& old = state[b];
        if (now.on == old.on && now.type == old.type && now.freq == old.freq
            && now.gain == old.gain && now.q == old.q)
            continue;

        state[b] = now;
        const auto coefs = dq::makeCoefs ((dq::FilterType) now.type, currentSampleRate,
                                          now.freq, now.gain, now.q);
        for (int ch = 0; ch < 2; ++ch)
            filters[b][ch].setCoefficients (coefs);
    }
}

void DodeligQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);

    updateFilters();

    for (int b = 0; b < dq::numBands; ++b)
        if (state[b].on)
            for (int ch = 0; ch < numChannels; ++ch)
                filters[b][ch].processSamples (buffer.getWritePointer (ch), numSamples);

    buffer.applyGain (juce::Decibels::decibelsToGain (outGainParam->load()));

    pushScope (buffer);
}

void DodeligQProcessor::pushScope (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    int s1, n1, s2, n2;
    scopeFifo.prepareToWrite (n, s1, n1, s2, n2);

    const float* L = buffer.getReadPointer (0);
    const float* R = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : L;

    for (int i = 0; i < n1; ++i)
        scopeBuffer[(size_t) (s1 + i)] = 0.5f * (L[i] + R[i]);
    for (int i = 0; i < n2; ++i)
        scopeBuffer[(size_t) (s2 + i)] = 0.5f * (L[n1 + i] + R[n1 + i]);

    scopeFifo.finishedWrite (n1 + n2);
}

juce::AudioProcessorEditor* DodeligQProcessor::createEditor()
{
    return new DodeligQEditor (*this);
}

void DodeligQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto s = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (s.createXml());
    copyXmlToBinary (*xml, destData);
}

void DodeligQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DodeligQProcessor();
}
