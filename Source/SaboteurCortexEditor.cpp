#include "SaboteurCortexEditor.h"

CortexEmeraldLookAndFeel::CortexEmeraldLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffc8f7e5));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff101816));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff233b34));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff14201c));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xff2ee59d));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1fa874));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff101816));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffc8f7e5));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff1fa874));
}

void CortexEmeraldLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, const float rotaryStartAngle,
    const float rotaryEndAngle, juce::Slider&)
{
    const float radius = static_cast<float> (juce::jmin(width / 2, height / 2)) - 6.0f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour(juce::Colour(0xff121a18));
    g.fillEllipse(rx, ry, rw, rw);
    g.setColour(juce::Colour(0xff28423a));
    g.drawEllipse(rx, ry, rw, rw, 1.5f);

    juce::Path bgArc;
    bgArc.addCentredArc(centreX, centreY, radius + 3.0f, radius + 3.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff1d2e29));
    g.strokePath(bgArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc;
    valArc.addCentredArc(centreX, centreY, radius + 3.0f, radius + 3.0f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(juce::Colour(0xfff5b041)); // Or ambré pour les contrôles transcrâniens
    g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.addRoundedRectangle(-1.5f, -radius + 3.0f, 3.0f, radius * 0.65f, 1.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(juce::Colour(0xffffdf9e));
    g.fillPath(pointer);
}

void CortexEmeraldLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    const juce::Slider::SliderStyle, juce::Slider&)
{
    const float trackX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float zeroY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;

    // Rail de fond
    g.setColour(juce::Colour(0xff1b2b26));
    g.drawVerticalLine(static_cast<int> (trackX), static_cast<float> (y), static_cast<float> (y + height));

    // Repère 0 dB
    g.setColour(juce::Colour(0xff3c5e53));
    g.drawHorizontalLine(static_cast<int> (zeroY), trackX - 8.0f, trackX + 8.0f);

    // Barre active depuis 0 dB
    g.setColour(juce::Colour(0xff2ee59d));
    float topY = std::min(zeroY, sliderPos);
    float botY = std::max(zeroY, sliderPos);
    g.fillRect(trackX - 2.0f, topY, 4.0f, std::max(2.0f, botY - topY));

    // Capuchon du fader
    juce::Rectangle<float> thumb(trackX - 11.0f, sliderPos - 5.0f, 22.0f, 10.0f);
    g.setColour(juce::Colour(0xff101816));
    g.fillRoundedRectangle(thumb, 3.0f);
    g.setColour(juce::Colour(0xff2ee59d));
    g.drawRoundedRectangle(thumb, 3.0f, 1.5f);
}

SaboteurCortexEditor::SaboteurCortexEditor(SaboteurCortexProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&cortexLookAndFeel);

    // Menus déroulants
    testModeBox.addItemList({
        "OFF (Listen Bus Active)",
        "1. Iso-Loudness (Ref 1kHz <-> Band)",
        "2. Transcranial L <-> R Ping-Pong",
        "3. Phase Coherence Lock (L+R Pulse)",
        "4. Continuous Sweep Check"
        }, 1);
    addAndMakeVisible(testModeBox);
    testModeAttach = std::make_unique<ComboAttachment>(processorRef.apvts, "test_mode", testModeBox);

    targetBandBox.addItemList({ "80 Hz", "160 Hz", "315 Hz", "630 Hz", "1.25 kHz", "2.5 kHz", "5 kHz", "10 kHz" }, 1);
    addAndMakeVisible(targetBandBox);
    targetBandAttach = std::make_unique<ComboAttachment>(processorRef.apvts, "target_band", targetBandBox);

    matrixBox.addItemList({
        "Stereo (Calibrated)",
        "Mono Sum (L+R Phase Safe)",
        "Side Reveal (L-R Stereo Width)",
        "Left Channel Only",
        "Right Channel Only"
        }, 1);
    addAndMakeVisible(matrixBox);
    matrixAttach = std::make_unique<ComboAttachment>(processorRef.apvts, "listen_matrix", matrixBox);

    bypassButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff5b041));
    bypassButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffff5533));
    addAndMakeVisible(bypassButton);
    bypassAttach = std::make_unique<ButtonAttachment>(processorRef.apvts, "eq_bypass", bypassButton);

    // Contrôles Transcrâniens Gauche -> Droite
    setupKnob(testLevelSlider, testLevelLabel, "TEST LEVEL");
    setupKnob(leftGainSlider, leftGainLabel, "L SKULL GAIN");
    setupKnob(leftTiltSlider, leftTiltLabel, "L HF SHELF");
    setupKnob(leftDelaySlider, leftDelayLabel, "L PHASE DELAY");

    testLevelAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "test_level", testLevelSlider);
    leftGainAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "left_gain", leftGainSlider);
    leftTiltAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "left_tilt", leftTiltSlider);
    leftDelayAttach = std::make_unique<SliderAttachment>(processorRef.apvts, "left_delay_ms", leftDelaySlider);

    leftInvertButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffdf9e));
    leftInvertButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xfff5b041));
    addAndMakeVisible(leftInvertButton);
    leftInvertAttach = std::make_unique<ButtonAttachment>(processorRef.apvts, "left_invert", leftInvertButton);

    // 8 Faders d'égalisation Iso-Sonique
    const char* bandIds[SaboteurCortexProcessor::numEqBands] = { "b80", "b160", "b315", "b630", "b1250", "b2500", "b5000", "b10000" };
    const char* bandNames[SaboteurCortexProcessor::numEqBands] = { "80", "160", "315", "630", "1.25k", "2.5k", "5k", "10k" };

    for (int i = 0; i < SaboteurCortexProcessor::numEqBands; ++i)
    {
        setupBandSlider(bandSliders[i], bandLabels[i], bandNames[i]);
        bandAttachments[i] = std::make_unique<SliderAttachment>(processorRef.apvts, bandIds[i], bandSliders[i]);
    }

    setSize(960, 580);
    startTimerHz(60);
}

SaboteurCortexEditor::~SaboteurCortexEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SaboteurCortexEditor::setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xfff5b041));
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    addAndMakeVisible(label);
}

void SaboteurCortexEditor::setupBandSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 18);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff2ee59d));
    label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    addAndMakeVisible(label);
}

void SaboteurCortexEditor::timerCallback()
{
    processorRef.getVisualSnapshot(guiSpectrum, guiCorrelation, guiLeftRms, guiRightRms, guiIsRefPlaying, guiSampleRate);
    repaint();
}

void SaboteurCortexEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bgGrad(juce::Colour(0xff0b110f), 0.0f, 0.0f,
        juce::Colour(0xff14201c), 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Bandeau supérieur
    auto header = getLocalBounds().removeFromTop(52);
    g.setColour(juce::Colour(0xff0e1714));
    g.fillRect(header);
    g.setColour(juce::Colour(0xff2ee59d).withAlpha(0.45f));
    g.drawHorizontalLine(51, 0.0f, static_cast<float> (getWidth()));

    g.setColour(juce::Colour(0xff2ee59d));
    g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
    g.drawText("SABOTEUR", 18, 10, 130, 32, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffc8f7e5));
    g.setFont(juce::FontOptions(19.0f, juce::Font::plain));
    g.drawText("// CORTEX", 136, 10, 120, 32, juce::Justification::centredLeft);

    // Écran central d'analyse (Spectre + Courbes Osseuses L/R + Corrélomètre)
    juce::Rectangle<int> screenBounds(18, 64, getWidth() - 36, 250);
    drawVisualizerAndPhaseMeter(g, screenBounds);

    // Cadres des deux sections inférieures
    juce::Rectangle<float> leftPanel(18.0f, 326.0f, 375.0f, 238.0f);
    juce::Rectangle<float> rightPanel(405.0f, 326.0f, 537.0f, 238.0f);

    g.setColour(juce::Colour(0xff0f1815));
    g.fillRoundedRectangle(leftPanel, 8.0f);
    g.fillRoundedRectangle(rightPanel, 8.0f);

    g.setColour(juce::Colour(0xfff5b041).withAlpha(0.4f));
    g.drawRoundedRectangle(leftPanel, 8.0f, 1.2f);
    g.drawText("TRANSCRANIAL LEFT -> RIGHT ALIGNMENT", 28, 332, 350, 18, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff2ee59d).withAlpha(0.4f));
    g.drawRoundedRectangle(rightPanel, 8.0f, 1.2f);
    g.drawText("ISO-LOUDNESS BONE CONDUCTION CURVE (8 BANDS)", 418, 332, 480, 18, juce::Justification::centredLeft);
}

void SaboteurCortexEditor::drawVisualizerAndPhaseMeter(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const float x0 = static_cast<float> (bounds.getX());
    const float y0 = static_cast<float> (bounds.getY());
    const float w = static_cast<float> (bounds.getWidth());
    const float h = static_cast<float> (bounds.getHeight()) - 34.0f; // Garde 34 px en bas pour le corrélomètre

    g.setColour(juce::Colour(0xff080d0b));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    const float minFreq = 40.0f;
    const float maxFreq = 16000.0f;
    auto freqToX = [&](float freq)
        {
            float norm = std::log(std::clamp(freq, minFreq, maxFreq) / minFreq) / std::log(maxFreq / minFreq);
            return x0 + norm * w;
        };

    // Grille verticale sur les 8 fréquences de test
    g.setFont(juce::FontOptions(11.0f));
    const int activeBand = targetBandBox.getSelectedItemIndex();
    const int activeTest = testModeBox.getSelectedItemIndex();

    for (int b = 0; b < SaboteurCortexProcessor::numEqBands; ++b)
    {
        float f = SaboteurCortexProcessor::bandFreqs[b];
        float gx = freqToX(f);

        if (activeTest == 1 && b == activeBand)
        {
            g.setColour(juce::Colour(0xff2ee59d).withAlpha(0.14f));
            g.fillRect(gx - 18.0f, y0, 36.0f, h);
        }

        g.setColour(juce::Colour(0xff182622));
        g.drawVerticalLine(static_cast<int> (gx), y0, y0 + h);
    }

    // Lignes horizontales d'égalisation (+12 dB, 0 dB, -12 dB)
    const float zeroY = y0 + h * 0.5f;
    g.setColour(juce::Colour(0xff263d36));
    g.drawHorizontalLine(static_cast<int> (zeroY), x0, x0 + w);

    // 1. Tracé du spectre audio en arrière-plan
    if (guiSpectrum.size() > SaboteurCortexProcessor::numBins)
    {
        juce::Path specPath;
        specPath.startNewSubPath(x0, y0 + h);
        const float binHz = static_cast<float> (guiSampleRate) / static_cast<float> (SaboteurCortexProcessor::fftSize);

        for (int i = 1; i <= SaboteurCortexProcessor::numBins; ++i)
        {
            float freq = i * binHz;
            if (freq < minFreq || freq > maxFreq) continue;
            float px = freqToX(freq);
            float db = juce::Decibels::gainToDecibels(guiSpectrum[i] + 1e-7f, -72.0f);
            float norm = std::clamp((db + 72.0f) / 72.0f, 0.0f, 1.0f);
            specPath.lineTo(px, (y0 + h) - norm * (h * 0.9f));
        }
        specPath.lineTo(x0 + w, y0 + h);
        specPath.closeSubPath();

        g.setColour(juce::Colour(0xff2ee59d).withAlpha(0.15f));
        g.fillPath(specPath);
    }

    // 2. Courbe de Correction Droite (Émeraude) et Gauche Transcrânienne (Or)
    juce::Path rightCurve, leftCurve;
    const int numSteps = 200;
    for (int s = 0; s <= numSteps; ++s)
    {
        float normX = static_cast<float> (s) / static_cast<float> (numSteps);
        float freq = minFreq * std::pow(maxFreq / minFreq, normX);
        float px = x0 + normX * w;

        float rDb = processorRef.getCorrectionGainDbAtFreq(freq, false);
        float lDb = processorRef.getCorrectionGainDbAtFreq(freq, true);

        float pyR = zeroY - (std::clamp(rDb, -24.0f, 24.0f) / 24.0f) * (h * 0.45f);
        float pyL = zeroY - (std::clamp(lDb, -24.0f, 24.0f) / 24.0f) * (h * 0.45f);

        if (s == 0)
        {
            rightCurve.startNewSubPath(px, pyR);
            leftCurve.startNewSubPath(px, pyL);
        }
        else
        {
            rightCurve.lineTo(px, pyR);
            leftCurve.lineTo(px, pyL);
        }
    }

    g.setColour(juce::Colour(0xfff5b041).withAlpha(0.85f));
    g.strokePath(leftCurve, juce::PathStrokeType(2.0f));

    g.setColour(juce::Colour(0xff2ee59d));
    g.strokePath(rightCurve, juce::PathStrokeType(2.4f));

    // Témoin interactif du générateur de calibration en haut à droite du graphe
    if (activeTest > 0)
    {
        juce::Rectangle<float> badge(x0 + w - 250.0f, y0 + 10.0f, 238.0f, 26.0f);
        g.setColour(guiIsRefPlaying ? juce::Colour(0xff1fa874) : juce::Colour(0xfff5b041));
        g.fillRoundedRectangle(badge, 5.0f);
        g.setColour(juce::Colour(0xff080d0b));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));

        juce::String statusTxt = "CALIBRATION ACTIVE";
        if (activeTest == 1)
            statusTxt = guiIsRefPlaying ? "PLAYING: REF 1 kHz" : "PLAYING: TARGET BAND";
        else if (activeTest == 2)
            statusTxt = guiIsRefPlaying ? "PING: LEFT TRANSDUCER" : "PONG: RIGHT TRANSDUCER";
        else if (activeTest == 3)
            statusTxt = "PHASE LOCK PULSE (L + R)";

        g.drawText(statusTxt, badge.toNearestInt(), juce::Justification::centred);
    }

    // 3. Barre inférieure : Corrélomètre de Phase Crânienne (-1.0 à +1.0)
    const float meterY = y0 + h + 6.0f;
    juce::Rectangle<float> corrBar(x0 + 170.0f, meterY, w - 185.0f, 20.0f);
    g.setColour(juce::Colour(0xff14201c));
    g.fillRoundedRectangle(corrBar, 4.0f);

    g.setColour(juce::Colour(0xffc8f7e5));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("SKULL PHASE COHERENCE :", static_cast<int> (x0 + 12), static_cast<int> (meterY), 155, 20, juce::Justification::centredLeft);

    const float centerCorrX = corrBar.getCentreX();
    const float corrPos = centerCorrX + (guiCorrelation * (corrBar.getWidth() * 0.48f));
    juce::Colour corrCol = (guiCorrelation >= 0.0f) ? juce::Colour(0xff2ee59d) : juce::Colour(0xffff4d4d);

    g.setColour(corrCol);
    float barLeft = std::min(centerCorrX, corrPos);
    float barWidth = std::max(4.0f, std::abs(corrPos - centerCorrX));
    g.fillRoundedRectangle(barLeft, meterY + 3.0f, barWidth, 14.0f, 3.0f);

    g.setColour(juce::Colour(0xff2ee59d).withAlpha(0.55f));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.5f);
}

void SaboteurCortexEditor::resized()
{
    // Barre supérieure
    testModeBox.setBounds(260, 12, 235, 28);
    targetBandBox.setBounds(505, 12, 95, 28);
    matrixBox.setBounds(610, 12, 190, 28);
    bypassButton.setBounds(810, 12, 135, 28);

    // Section Gauche : 4 Rotatifs Transcrâniens + Bouton Polarité
    const int knobY = 378;
    const int knobW = 84;
    const int knobH = 96;
    juce::Slider* leftKnobs[] = { &testLevelSlider, &leftGainSlider, &leftTiltSlider, &leftDelaySlider };
    juce::Label* leftLbls[] = { &testLevelLabel,  &leftGainLabel,  &leftTiltLabel,  &leftDelayLabel };

    for (int i = 0; i < 4; ++i)
    {
        int kx = 26 + i * 88;
        leftLbls[i]->setBounds(kx, knobY - 20, knobW, 18);
        leftKnobs[i]->setBounds(kx, knobY, knobW, knobH);
    }
    leftInvertButton.setBounds(120, 492, 180, 26);

    // Section Droite : 8 Faders d'égalisation Iso-Sonique
    const int startX = 418;
    const int spacing = 64;
    for (int b = 0; b < SaboteurCortexProcessor::numEqBands; ++b)
    {
        int bx = startX + b * spacing;
        bandLabels[b].setBounds(bx, 356, 56, 18);
        bandSliders[b].setBounds(bx, 376, 56, 175);
    }
}