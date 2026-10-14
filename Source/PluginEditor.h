#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SusurroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SusurroLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class SusurroAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SusurroAudioProcessorEditor (SusurroAudioProcessor&);
    ~SusurroAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (Knob&, const juce::String& paramId, const juce::String& name);

    SusurroAudioProcessor& proc;
    SusurroLookAndFeel lnf;
    Knob attack, release, pressure, air, tracking;
    Knob eq1, eq2, eq3, eq4, eq5;
    Knob bands, formant, width, mix, output;
    std::vector<Knob*> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SusurroAudioProcessorEditor)
};
