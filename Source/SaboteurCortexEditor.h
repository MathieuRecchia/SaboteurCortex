#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SaboteurCortexProcessor.h"

class CortexEmeraldLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CortexEmeraldLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, const float rotaryStartAngle,
        const float rotaryEndAngle, juce::Slider& slider) override;
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override;
};

class SaboteurCortexEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit SaboteurCortexEditor(SaboteurCortexProcessor&);
    ~SaboteurCortexEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawVisualizerAndPhaseMeter(juce::Graphics& g, juce::Rectangle<int> bounds);
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void setupBandSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);

    SaboteurCortexProcessor& processorRef;
    CortexEmeraldLookAndFeel cortexLookAndFeel;

    // Bandeau supérieur (Calibration & Matrice d'écoute)
    juce::ComboBox testModeBox;
    juce::ComboBox targetBandBox;
    juce::ComboBox matrixBox;
    juce::ToggleButton bypassButton{ "BYPASS CALIBRATION" };

    // Section Gauche : Compensation Transcrânienne L -> R & Niveau de Test
    juce::Slider testLevelSlider;
    juce::Slider leftGainSlider, leftTiltSlider, leftDelaySlider;
    juce::Label  testLevelLabel, leftGainLabel, leftTiltLabel, leftDelayLabel;
    juce::ToggleButton leftInvertButton{ "INVERT L PHASE" };

    // Section Droite : Les 8 faders verticaux d'égalisation Iso-Sonique
    std::array<juce::Slider, SaboteurCortexProcessor::numEqBands> bandSliders;
    std::array<juce::Label, SaboteurCortexProcessor::numEqBands> bandLabels;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAttachment>  testModeAttach, targetBandAttach, matrixAttach;
    std::unique_ptr<ButtonAttachment> bypassAttach, leftInvertAttach;
    std::unique_ptr<SliderAttachment> testLevelAttach, leftGainAttach, leftTiltAttach, leftDelayAttach;
    std::array<std::unique_ptr<SliderAttachment>, SaboteurCortexProcessor::numEqBands> bandAttachments;

    // Données graphiques 60 FPS
    std::vector<float> guiSpectrum;
    float guiCorrelation = 1.0f;
    float guiLeftRms = 0.0f;
    float guiRightRms = 0.0f;
    bool guiIsRefPlaying = false;
    double guiSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaboteurCortexEditor)
};