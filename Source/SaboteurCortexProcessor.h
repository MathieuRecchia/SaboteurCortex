#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

class SaboteurCortexProcessor : public juce::AudioProcessor
{
public:
    SaboteurCortexProcessor();
    ~SaboteurCortexProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // --- MÉTHODES STANDARD JUCE OBLIGATOIRES ---
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int numEqBands = 8;
    static constexpr std::array<float, numEqBands> bandFreqs{
        80.0f, 160.0f, 315.0f, 630.0f, 1250.0f, 2500.0f, 5000.0f, 10000.0f
    };

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int numBins = fftSize / 2;   // 1024

    // Calcul de la réponse en fréquence de la courbe de correction pour le GUI
    float getCorrectionGainDbAtFreq(float freqHz, bool isLeftChannel) const;

    void getVisualSnapshot(std::vector<float>& outSpectrum,
        float& outPhaseCorrelation,
        float& outLeftRms,
        float& outRightRms,
        bool& outIsRefTonePlaying,
        double& outSampleRate);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct SimpleBiquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }

        float process(float x) noexcept
        {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void setPeaking(double sr, float freq, float q, float gainDb) noexcept
        {
            if (std::abs(gainDb) < 0.01f)
            {
                b0 = 1.0f; b1 = 0.0f; b2 = 0.0f; a1 = 0.0f; a2 = 0.0f;
                return;
            }
            float A = std::pow(10.0f, gainDb / 40.0f);
            float w0 = juce::MathConstants<float>::twoPi * std::clamp(freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos(w0), sinw = std::sin(w0);
            float alpha = sinw / (2.0f * q);
            float a0Inv = 1.0f / (1.0f + alpha / A);
            b0 = (1.0f + alpha * A) * a0Inv;
            b1 = (-2.0f * cosw) * a0Inv;
            b2 = (1.0f - alpha * A) * a0Inv;
            a1 = b1;
            a2 = (1.0f - alpha / A) * a0Inv;
        }

        void setHighShelf(double sr, float freq, float q, float gainDb) noexcept
        {
            if (std::abs(gainDb) < 0.01f)
            {
                b0 = 1.0f; b1 = 0.0f; b2 = 0.0f; a1 = 0.0f; a2 = 0.0f;
                return;
            }
            float A = std::pow(10.0f, gainDb / 40.0f);
            float w0 = juce::MathConstants<float>::twoPi * std::clamp(freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos(w0), sinw = std::sin(w0);
            float alpha = sinw / (2.0f * q);
            float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
            float a0Inv = 1.0f / ((A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha);
            b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha)) * a0Inv;
            b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw)) * a0Inv;
            b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha)) * a0Inv;
            a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosw)) * a0Inv;
            a2 = ((A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha) * a0Inv;
        }
    };

    struct ChannelFilterChain
    {
        std::array<SimpleBiquad, numEqBands> eqBands;
        SimpleBiquad transcranialShelf;

        void reset() noexcept
        {
            for (auto& b : eqBands) b.reset();
            transcranialShelf.reset();
        }
    };

    double currentSampleRate = 48000.0;
    std::array<ChannelFilterChain, 2> filterChains;

    // Ligne à retard fractionnaire pour l'alignement de phase crânien du canal Gauche (jusqu'à 5 ms)
    std::vector<float> leftDelayBuffer;
    int delayWritePos = 0;

    // Générateur de signaux d'audiométrie (Warble Tone + Comparateur 1 kHz)
    double oscPhase = 0.0;
    double warblePhase = 0.0;
    double pulsePhase = 0.0;
    uint32_t noiseSeed = 22222u;

    // Analyseur FFT et Corrélomètre de phase temps réel
    juce::dsp::FFT visFft;
    juce::dsp::WindowingFunction<float> visWindow;
    std::vector<float> visFifo;
    std::vector<float> fftWorkspace;
    int visFifoIndex = 0;

    juce::SpinLock visualLock;
    std::vector<float> visSpectrum;
    float visCorrelation = 1.0f;
    float visLeftRms = 0.0f;
    float visRightRms = 0.0f;
    bool visIsRefPlaying = false;

    void updateFilters(const std::array<float, numEqBands>& gainsDb, float leftShelfDb);
    float generatePinkishNoise() noexcept;
    void pushVisualSample(float monoOut);
    void computeVisualFft();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaboteurCortexProcessor)
};