#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
const juce::Colour bandColours[dq::numBands] {
    juce::Colour (0xffff4d4d), juce::Colour (0xffff9f1a), juce::Colour (0xffffd21a), juce::Colour (0xff7ddc3a),
    juce::Colour (0xff1ad6c4), juce::Colour (0xff2a8cff), juce::Colour (0xff9a5cff), juce::Colour (0xffff4fb3)
};

const juce::Colour topCol    (0xff3b3e45);
const juce::Colour bottomCol (0xff1f2126);
const juce::Colour plotCol   (0xff16181c);
const juce::Colour gridCol   (0xff343840);
const juce::Colour labelCol  (0xff9ca3ad);
const juce::Colour curveCol  (0xffffd21a);
const juce::Colour posCol    (0xff9a4dff);
const juce::Colour negCol    (0xff2a7fff);

constexpr float maxDb = 30.0f;
constexpr int editorWidth = 920;
constexpr int editorHeight = 600;
}

DodeligQEditor::DodeligQEditor (DodeligQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), fft (DodeligQProcessor::fftOrder)
{
    setSize (editorWidth, editorHeight);

    for (int i = 0; i < dq::numBands; ++i)
    {
        auto& b = bandButtons[i];
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.setColour (juce::TextButton::buttonOnColourId, bandColours[i]);
        b.setColour (juce::TextButton::buttonColourId, bandColours[i].darker (0.7f));
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.onClick = [this, i] { selectBand (i); };
        addAndMakeVisible (b);
    }

    typeBox.addItem ("Campana", 1);
    typeBox.addItem ("Estante grave", 2);
    typeBox.addItem ("Estante agudo", 3);
    typeBox.addItem ("Paso alto", 4);
    typeBox.addItem ("Paso bajo", 5);
    typeBox.addItem ("Muesca", 6);
    addAndMakeVisible (typeBox);

    onButton.setButtonText ("Activa");
    addAndMakeVisible (onButton);

    juce::Slider* knobs[] { &freqSlider, &gainSlider, &qSlider };
    for (auto* k : knobs)
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k->setColour (juce::Slider::rotarySliderFillColourId, curveCol);
        k->setColour (juce::Slider::rotarySliderOutlineColourId, plotCol);
        k->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        addAndMakeVisible (*k);
    }

    outSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    outSlider.setColour (juce::Slider::trackColourId, curveCol);
    outSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible (outSlider);
    outAtt = std::make_unique<SliderAtt> (proc.apvts, "outGain", outSlider);

    selectBand (0);
    startTimerHz (30);
}

DodeligQEditor::~DodeligQEditor()
{
    stopTimer();
}

// ---------- Parámetros ----------

float DodeligQEditor::bandParam (int b, const char* suffix) const
{
    return proc.apvts.getRawParameterValue ("b" + juce::String (b + 1) + "_" + suffix)->load();
}

void DodeligQEditor::setBandParam (int b, const char* suffix, float value)
{
    if (auto* p = proc.apvts.getParameter ("b" + juce::String (b + 1) + "_" + suffix))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void DodeligQEditor::selectBand (int index)
{
    selected = index;
    bandButtons[index].setToggleState (true, juce::dontSendNotification);

    const auto prefix = "b" + juce::String (index + 1) + "_";
    typeAtt.reset();
    onAtt.reset();
    freqAtt.reset();
    gainAtt.reset();
    qAtt.reset();

    typeAtt = std::make_unique<ComboAtt>  (proc.apvts, prefix + "type", typeBox);
    onAtt   = std::make_unique<ButtonAtt> (proc.apvts, prefix + "on", onButton);
    freqAtt = std::make_unique<SliderAtt> (proc.apvts, prefix + "freq", freqSlider);
    gainAtt = std::make_unique<SliderAtt> (proc.apvts, prefix + "gain", gainSlider);
    qAtt    = std::make_unique<SliderAtt> (proc.apvts, prefix + "q", qSlider);
    repaint();
}

// ---------- Mapeo de coordenadas ----------

juce::Rectangle<float> DodeligQEditor::plotArea() const
{
    return { 12.0f, 48.0f, 896.0f, 316.0f };
}

float DodeligQEditor::xFromFreq (float f) const
{
    const auto p = plotArea();
    return p.getX() + std::log (f / 20.0f) / std::log (1000.0f) * p.getWidth();
}

float DodeligQEditor::freqFromX (float x) const
{
    const auto p = plotArea();
    return 20.0f * std::pow (1000.0f, (x - p.getX()) / p.getWidth());
}

float DodeligQEditor::yFromGain (float g) const
{
    const auto p = plotArea();
    return p.getCentreY() - g / maxDb * (p.getHeight() * 0.5f);
}

float DodeligQEditor::gainFromY (float y) const
{
    const auto p = plotArea();
    return (p.getCentreY() - y) / (p.getHeight() * 0.5f) * maxDb;
}

float DodeligQEditor::yFromSpec (float db) const
{
    const auto p = plotArea();
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 80.0f);
    return p.getBottom() - norm * p.getHeight();
}

float DodeligQEditor::nodeY (int b) const
{
    const auto type = (int) bandParam (b, "type");
    return typeHasGain (type) ? yFromGain (bandParam (b, "gain")) : plotArea().getCentreY();
}

int DodeligQEditor::nodeAt (juce::Point<float> pos) const
{
    int best = -1;
    float bestDist = 14.0f;
    for (int b = 0; b < dq::numBands; ++b)
    {
        const juce::Point<float> np (xFromFreq (bandParam (b, "freq")), nodeY (b));
        const float d = np.getDistanceFrom (pos);
        if (d < bestDist)
        {
            bestDist = d;
            best = b;
        }
    }
    return best;
}

// ---------- Analizador ----------

void DodeligQEditor::readScope()
{
    auto& fifo = proc.scopeFifo;
    int s1, n1, s2, n2;
    fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);

    for (int i = 0; i < n1; ++i)
    {
        ring[(size_t) ringPos] = proc.scopeBuffer[(size_t) (s1 + i)];
        ringPos = (ringPos + 1) & (DodeligQProcessor::fftSize - 1);
    }
    for (int i = 0; i < n2; ++i)
    {
        ring[(size_t) ringPos] = proc.scopeBuffer[(size_t) (s2 + i)];
        ringPos = (ringPos + 1) & (DodeligQProcessor::fftSize - 1);
    }
    fifo.finishedRead (n1 + n2);
}

void DodeligQEditor::computeSpectrum()
{
    constexpr int N = DodeligQProcessor::fftSize;

    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) & (N - 1))] * w;
    }
    std::fill (fftData.begin() + N, fftData.end(), 0.0f);

    fft.performFrequencyOnlyForwardTransform (fftData.data());

    for (int k = 0; k < N / 2; ++k)
    {
        const float mag = fftData[(size_t) k] / (N * 0.5f);
        const float db = juce::Decibels::gainToDecibels (mag, -120.0f);
        spectrum[(size_t) k] = 0.6f * spectrum[(size_t) k] + 0.4f * db;
    }
}

void DodeligQEditor::timerCallback()
{
    readScope();
    computeSpectrum();
    for (int b = 0; b < dq::numBands; ++b)
        bandButtons[b].setAlpha (bandParam (b, "on") > 0.5f ? 1.0f : 0.45f);
    repaint();
}

// ---------- Ratón ----------

void DodeligQEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! plotArea().contains (e.position))
        return;
    dragging = nodeAt (e.position);
    if (dragging >= 0)
        selectBand (dragging);
}

void DodeligQEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const auto plot = plotArea();
    const float x = juce::jlimit (plot.getX(), plot.getRight(), e.position.x);

    setBandParam (dragging, "freq", juce::jlimit (10.0f, 30000.0f, freqFromX (x)));
    if (typeHasGain ((int) bandParam (dragging, "type")))
        setBandParam (dragging, "gain", juce::jlimit (-maxDb, maxDb, gainFromY (e.position.y)));
    setBandParam (dragging, "on", 1.0f);
}

void DodeligQEditor::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
}

void DodeligQEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! plotArea().contains (e.position))
        return;
    const float q = juce::jlimit (0.1f, 18.0f, bandParam (selected, "q") * std::pow (2.0f, w.deltaY * 4.0f));
    setBandParam (selected, "q", q);
}

// ---------- Dibujo ----------

void DodeligQEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (topCol, 0.0f, 0.0f, bottomCol, 0.0f, (float) getHeight(), false));
    g.fillAll();

    const auto plot = plotArea();
    const float sr = (float) std::max (1.0, proc.getSampleRate());
    constexpr int N = DodeligQProcessor::fftSize;

    // Cabecera
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText ("dodeligQ-1", 16, 8, 240, 26, juce::Justification::left);
    g.setColour (labelCol);
    g.setFont (12.0f);
    g.drawText ("Ecualizador paramétrico · 8 bandas", 16, 26, 300, 16, juce::Justification::left);
    g.drawText ("SALIDA", 580, 14, 70, 18, juce::Justification::right);

    // Fondo del gráfico y rejilla
    g.setColour (plotCol);
    g.fillRoundedRectangle (plot, 6.0f);

    g.setColour (gridCol);
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine ((int) xFromFreq (f), plot.getY(), plot.getBottom());
    for (float db : { -24.0f, -12.0f, 12.0f, 24.0f })
        g.drawHorizontalLine ((int) yFromGain (db), plot.getX(), plot.getRight());
    g.setColour (gridCol.brighter (0.4f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

    // Analizador
    juce::Path specPath;
    specPath.startNewSubPath (plot.getX(), plot.getBottom());
    for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
    {
        const int k = juce::jlimit (1, N / 2 - 1, (int) (freqFromX (x) * N / sr));
        specPath.lineTo (x, yFromSpec (spectrum[(size_t) k]));
    }
    specPath.lineTo (plot.getRight(), plot.getBottom());
    specPath.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillPath (specPath);

    // Respuesta de las bandas activas
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, dq::numBands> coefs;
    std::array<bool, dq::numBands> active {};
    for (int b = 0; b < dq::numBands; ++b)
    {
        active[(size_t) b] = bandParam (b, "on") > 0.5f;
        if (active[(size_t) b])
            coefs[(size_t) b] = dq::makeCoefs ((dq::FilterType) (int) bandParam (b, "type"), sr,
                                               bandParam (b, "freq"), bandParam (b, "gain"), bandParam (b, "q"));
    }

    juce::Path curve;
    bool first = true;
    for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
    {
        const double f = freqFromX (x);
        double totalDb = 0.0;
        for (int b = 0; b < dq::numBands; ++b)
            if (active[(size_t) b])
                totalDb += juce::Decibels::gainToDecibels (coefs[(size_t) b]->getMagnitudeForFrequency (f, sr), -120.0);

        const float y = yFromGain (juce::jlimit (-maxDb, maxDb, (float) totalDb));
        if (first) { curve.startNewSubPath (x, y); first = false; }
        else       curve.lineTo (x, y);
    }

    juce::Path area (curve);
    area.lineTo (plot.getRight(), plot.getCentreY());
    area.lineTo (plot.getX(), plot.getCentreY());
    area.closeSubPath();

    g.setGradientFill (juce::ColourGradient (posCol.withAlpha (0.55f), 0.0f, plot.getY(),
                                             negCol.withAlpha (0.55f), 0.0f, plot.getBottom(), false));
    g.fillPath (area);
    g.setColour (curveCol);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Nodos de banda
    for (int b = 0; b < dq::numBands; ++b)
    {
        const float x = xFromFreq (bandParam (b, "freq"));
        const float y = nodeY (b);
        g.setColour (bandColours[b].withAlpha (active[(size_t) b] ? 1.0f : 0.35f));
        g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
        if (b == selected)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (x - 9.0f, y - 9.0f, 18.0f, 18.0f, 1.5f);
        }
    }

    // Etiquetas de ejes
    g.setColour (labelCol);
    g.setFont (11.0f);
    for (auto [f, text] : { std::pair { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
        g.drawText (text, (int) xFromFreq (f) - 20, (int) plot.getBottom() - 16, 40, 14, juce::Justification::centred);
    for (auto [db, text] : { std::pair { 30.0f, "+30" }, { 0.0f, "0" }, { -30.0f, "-30" } })
        g.drawText (text, (int) plot.getRight() - 34, (int) yFromGain (db) - 7, 30, 14, juce::Justification::right);

    // Panel inferior
    g.setColour (juce::Colour (0xff1a1c20));
    g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 374.0f, 896.0f, 214.0f), 8.0f);

    g.setColour (labelCol);
    g.setFont (11.0f);
    g.drawText ("FRECUENCIA", 20, 428, 150, 16, juce::Justification::centred);
    g.drawText ("GANANCIA",   190, 428, 150, 16, juce::Justification::centred);
    g.drawText ("Q",          360, 428, 150, 16, juce::Justification::centred);
}

void DodeligQEditor::resized()
{
    for (int i = 0; i < dq::numBands; ++i)
        bandButtons[i].setBounds (20 + i * 52, 384, 44, 26);

    typeBox.setBounds (470, 384, 170, 26);
    onButton.setBounds (660, 384, 90, 26);

    freqSlider.setBounds (20, 446, 150, 130);
    gainSlider.setBounds (190, 446, 150, 130);
    qSlider.setBounds (360, 446, 150, 130);

    outSlider.setBounds (660, 12, 244, 22);
}
