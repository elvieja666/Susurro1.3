#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      (0xff1b1c1f);
    const juce::Colour kPanel   (0xff26272b);
    const juce::Colour kAccent  (0xff9fc6d8);   // azul grisáceo, "aire"
    const juce::Colour kText    (0xffd8d8d8);
    const juce::Colour kDim     (0xff6b6d73);
}

SusurroLookAndFeel::SusurroLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
}

void SusurroLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = 3.5f;

    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (kDim.withAlpha (0.5f));
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    g.setColour (kAccent);
    g.strokePath (val, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - track * 3.0f;
    g.setColour (kPanel.brighter (0.15f));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 4.0f, 3.0f, knobR * 0.45f, 1.5f);
    g.setColour (kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

SusurroAudioProcessorEditor::SusurroAudioProcessorEditor (SusurroAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    // Fila 1: seguimiento de la voz + filtro del ruido
    addKnob (attack,   "attack",   "SMOOTHING");
    addKnob (release,  "release",  "RELEASE");
    addKnob (pressure, "pressure", "PRESSURE");
    addKnob (air,      "air",      "NOISE HPF");
    addKnob (tracking, "tracking", "TRACKING");
    // Fila 2: EQ del ruido (frecuencias del preset Whisper)
    addKnob (eq1, "eq1", "258 Hz");
    addKnob (eq2, "eq2", "689 Hz");
    addKnob (eq3, "eq3", "1.8 kHz");
    addKnob (eq4, "eq4", "4.4 kHz");
    addKnob (eq5, "eq5", "11 kHz");
    // Fila 3: vocoder (solo con Tracking > 0) + salida
    addKnob (bands,   "bands",   "BANDS");
    addKnob (formant, "formant", "FORMANT");
    addKnob (width,   "width",   "WIDTH");
    addKnob (mix,     "mix",     "MIX");
    addKnob (output,  "output",  "OUTPUT");
    setSize (640, 560);
}

SusurroAudioProcessorEditor::~SusurroAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SusurroAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, kDim.brighter (0.4f));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
    knobs.push_back (&k);
}

void SusurroAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    auto area = getLocalBounds().reduced (14);
    auto header = area.removeFromTop (44);

    g.setColour (kText);
    g.setFont (juce::FontOptions (26.0f, juce::Font::plain));
    g.drawText ("susurro", header.removeFromLeft (200), juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("whisper / noise vocoder", header, juce::Justification::centredRight);

    // Separadores entre filas
    auto inner = area.reduced (10, 10);
    const int rowH = inner.getHeight() / 3;
    g.setColour (kDim.withAlpha (0.25f));
    for (int r = 1; r < 3; ++r)
        g.drawHorizontalLine (inner.getY() + r * rowH, (float) inner.getX() + 8.0f, (float) inner.getRight() - 8.0f);

    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
}

void SusurroAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (44);
    area.reduce (10, 10);

    const int cols = 5, rows = 3;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / rows;

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int r = (int) i / cols, c = (int) i % cols;
        auto cell = juce::Rectangle<int> (area.getX() + c * cellW, area.getY() + r * cellH, cellW, cellH).reduced (6);
        knobs[i]->label.setBounds (cell.removeFromTop (18));
        knobs[i]->slider.setBounds (cell);
    }
}
