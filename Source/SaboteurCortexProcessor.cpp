#include "SaboteurCortexProcessor.h"
#include "SaboteurCortexEditor.h"

SaboteurCortexProcessor::SaboteurCortexProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout()),
    visFft(fftOrder),
    visWindow(fftSize, juce::dsp::WindowingFunction<float>::hann, false)
{
    leftDelayBuffer.assign(2048, 0.0f);
    visFifo.assign(fftSize, 0.0f);
    fftWorkspace.assign(fftSize * 2, 0.0f);
    visSpectrum.assign(numBins + 1, 0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout SaboteurCortexProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Mode du générateur de calibration
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "test_mode", "Calibration Generator",
        juce::StringArray{
            "OFF (Listen Bus Active)",
            "1. Iso-Loudness (Ref 1kHz <-> Band)",
            "2. Transcranial L <-> R Ping-Pong",
            "3. Phase Coherence Lock (L+R Pulse)",
            "4. Continuous Sweep Check"
        }, 0));

    // Bande sélectionnée pendant le test Iso-Loudness
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "target_band", "Test Target Band",
        juce::StringArray{ "80 Hz", "160 Hz", "315 Hz", "630 Hz", "1.25 kHz", "2.5 kHz", "5 kHz", "10 kHz" }, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "test_level", "Test Tone Level",
        juce::NormalisableRange<float>(-48.0f, -6.0f, 0.5f), -20.0f, "dB"));

    // 2. Matrice de Monitoring spécialisée Cophose Unilatérale
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "listen_matrix", "Monitoring Matrix",
        juce::StringArray{
            "Stereo (Calibrated)",
            "Mono Sum (L+R Phase Safe)",
            "Side Reveal (L-R Stereo Width)",
            "Left Channel Only",
            "Right Channel Only"
        }, 0));

    // 3. Compensation Transcrânienne Gauche -> Cochlée Droite
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "left_gain", "L Transcranial Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "left_tilt", "L Skull HF Shelf (>1.5k)",
        juce::NormalisableRange<float>(-12.0f, 15.0f, 0.1f), 0.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "left_delay_ms", "L Phase Align Delay",
        juce::NormalisableRange<float>(0.0f, 2.50f, 0.01f), 0.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "left_invert", "Invert Left Polarity", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "eq_bypass", "Bypass Correction Curve", false));

    // 4. Les 8 bandes d'égalisation de calibration (80 Hz -> 10 kHz)
    const char* bandIds[numEqBands] = { "b80", "b160", "b315", "b630", "b1250", "b2500", "b5000", "b10000" };
    const char* bandNames[numEqBands] = { "80 Hz", "160 Hz", "315 Hz", "630 Hz", "1.25 kHz", "2.5 kHz", "5 kHz", "10 kHz" };

    for (int i = 0; i < numEqBands; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandIds[i], bandNames[i],
            juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB"));
    }

    return { params.begin(), params.end() };
}

bool SaboteurCortexProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SaboteurCortexProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;

    for (auto& chain : filterChains)
        chain.reset();

    const int maxDelaySamples = static_cast<int> (currentSampleRate * 0.010) + 64; // 10 ms max
    leftDelayBuffer.assign(maxDelaySamples, 0.0f);
    delayWritePos = 0;

    oscPhase = 0.0;
    warblePhase = 0.0;
    pulsePhase = 0.0;

    visFifoIndex = 0;
    std::fill(visFifo.begin(), visFifo.end(), 0.0f);

    setLatencySamples(0);
}

void SaboteurCortexProcessor::releaseResources()
{
}

void SaboteurCortexProcessor::updateFilters(const std::array<float, numEqBands>& gainsDb, float leftShelfDb)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int b = 0; b < numEqBands; ++b)
            filterChains[ch].eqBands[b].setPeaking(currentSampleRate, bandFreqs[b], 1.41f, gainsDb[b]);

        // Compensation haute fréquence du crâne uniquement sur le canal Gauche (ch == 0)
        float shelfGain = (ch == 0) ? leftShelfDb : 0.0f;
        filterChains[ch].transcranialShelf.setHighShelf(currentSampleRate, 1600.0f, 0.7071f, shelfGain);
    }
}

float SaboteurCortexProcessor::generatePinkishNoise() noexcept
{
    noiseSeed = noiseSeed * 196314165u + 907633515u;
    float white = (static_cast<float> (noiseSeed) / static_cast<float> (0xFFFFFFFFu)) * 2.0f - 1.0f;
    return white * 0.5f;
}

void SaboteurCortexProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Lecture des paramètres
    const int testMode = static_cast<int>(apvts.getRawParameterValue("test_mode")->load());
    const int targetBandIdx = static_cast<int>(apvts.getRawParameterValue("target_band")->load());
    const float testLevelLin = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("test_level")->load());
    const int listenMatrix = static_cast<int>(apvts.getRawParameterValue("listen_matrix")->load());

    const float leftGainLin = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("left_gain")->load());
    const float leftShelfDb = apvts.getRawParameterValue("left_tilt")->load();
    const float leftDelayMs = apvts.getRawParameterValue("left_delay_ms")->load();
    const bool leftInvert = apvts.getRawParameterValue("left_invert")->load() > 0.5f;
    const bool eqBypass = apvts.getRawParameterValue("eq_bypass")->load() > 0.5f;

    const char* bandIds[numEqBands] = { "b80", "b160", "b315", "b630", "b1250", "b2500", "b5000", "b10000" };
    std::array<float, numEqBands> bandGains{};
    for (int b = 0; b < numEqBands; ++b)
        bandGains[b] = eqBypass ? 0.0f : apvts.getRawParameterValue(bandIds[b])->load();

    updateFilters(bandGains, eqBypass ? 0.0f : leftShelfDb);

    const float delaySamples = (leftDelayMs * 0.001f) * static_cast<float>(currentSampleRate);
    const int delayBufSize = static_cast<int>(leftDelayBuffer.size());

    auto* leftCh = buffer.getWritePointer(0);
    auto* rightCh = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    float sumLR = 0.0f, sumLL = 1e-7f, sumRR = 1e-7f;
    bool refPlayingSnap = false;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = leftCh[i];
        float inR = (rightCh != nullptr) ? rightCh[i] : inL;

        // --- 1. GÉNÉRATEUR DE TESTS AUDIOMÉTRIQUES OSSEUX (SI ACTIF) ---
        if (testMode > 0)
        {
            // Cycle d'alternance de 1.2 seconde (0.0 -> 0.5 = Référence / Gauche, 0.5 -> 1.0 = Cible / Droite)
            pulsePhase += 1.0 / (currentSampleRate * 1.2);
            if (pulsePhase >= 1.0) pulsePhase -= 1.0;

            const bool firstHalf = (pulsePhase < 0.5);
            refPlayingSnap = firstHalf;

            // Enveloppe douce anti-clic sur chaque bip
            double localPos = firstHalf ? (pulsePhase * 2.0) : ((pulsePhase - 0.5) * 2.0);
            float gateEnv = 1.0f;
            if (localPos < 0.08)      gateEnv = static_cast<float>(localPos / 0.08);
            else if (localPos > 0.88) gateEnv = static_cast<float>(std::max(0.0, (0.96 - localPos) / 0.08));
            if (localPos >= 0.96)     gateEnv = 0.0f;

            // Warble Tone (modulation FM +/- 5% à 5 Hz pour éviter les résonances stationnaires du crâne)
            warblePhase += 5.0 / currentSampleRate;
            if (warblePhase >= 1.0) warblePhase -= 1.0;
            const double warbleMod = 1.0 + 0.05 * std::sin(juce::MathConstants<double>::twoPi * warblePhase);

            double baseFreq = 1000.0;
            if (testMode == 1) // Iso-Loudness : alterne entre 1 kHz (Réf) et la bande choisie
            {
                baseFreq = firstHalf ? 1000.0 : static_cast<double> (bandFreqs[std::clamp(targetBandIdx, 0, numEqBands - 1)]);
            }
            else if (testMode == 2) // Ping-Pong Transcrânien L <-> R à la fréquence choisie
            {
                baseFreq = static_cast<double> (bandFreqs[std::clamp(targetBandIdx, 0, numEqBands - 1)]);
            }
            else if (testMode == 3) // Alignement de phase L+R (salve riche 500 Hz + 1.5 kHz)
            {
                baseFreq = 630.0;
            }
            else if (testMode == 4) // Balayage continu logarithmique 60 Hz -> 10 kHz
            {
                baseFreq = 60.0 * std::pow(10000.0 / 60.0, pulsePhase);
                gateEnv = 1.0f;
            }

            oscPhase += (baseFreq * warbleMod) / currentSampleRate;
            if (oscPhase >= 1.0) oscPhase -= 1.0;

            float sig = static_cast<float> (std::sin(juce::MathConstants<double>::twoPi * oscPhase));
            if (testMode == 3)
            {
                // Signal composite (fondamentale + 2e et 3e harmoniques + léger bruit) idéal pour sentir l'annulation de phase
                sig = 0.55f * sig
                    + 0.30f * static_cast<float> (std::sin(juce::MathConstants<double>::twoPi * oscPhase * 2.0))
                    + 0.15f * generatePinkishNoise();
            }

            sig *= testLevelLin * gateEnv;

            if (testMode == 2) // Ping-Pong Gauche puis Droite
            {
                inL = firstHalf ? sig : 0.0f;
                inR = firstHalf ? 0.0f : sig;
            }
            else
            {
                inL = sig;
                inR = sig;
            }
        }

        // --- 2. LIGNE À RETARD FRACTIONNAIRE & POLARITÉ (ALIGNEMENT TRANSCÂNIEN GAUCHE) ---
        leftDelayBuffer[delayWritePos] = inL;
        if (!eqBypass && delaySamples > 0.001f)
        {
            float readPos = static_cast<float> (delayWritePos) - delaySamples;
            while (readPos < 0.0f) readPos += static_cast<float>(delayBufSize);

            int idx0 = static_cast<int>(readPos) % delayBufSize;
            int idx1 = (idx0 + 1) % delayBufSize;
            float frac = readPos - std::floor(readPos);
            inL = leftDelayBuffer[idx0] * (1.0f - frac) + leftDelayBuffer[idx1] * frac;
        }
        delayWritePos = (delayWritePos + 1) % delayBufSize;

        if (!eqBypass)
        {
            if (leftInvert) inL = -inL;
            inL *= leftGainLin;
        }

        // --- 3. FILTRAGE DE CALIBRATION ISO-SONIQUE & SHELF CRÂNIEN ---
        float corrL = filterChains[0].transcranialShelf.process(inL);
        float corrR = filterChains[1].transcranialShelf.process(inR);

        for (int b = 0; b < numEqBands; ++b)
        {
            corrL = filterChains[0].eqBands[b].process(corrL);
            corrR = filterChains[1].eqBands[b].process(corrR);
        }

        // Mesure de la corrélation de phase juste avant la matrice d'écoute
        sumLR += corrL * corrR;
        sumLL += corrL * corrL;
        sumRR += corrR * corrR;

        // --- 4. MATRICE D'ÉCOUTE "SINGLE-COCHLEA" ---
        float outL = corrL;
        float outR = corrR;

        if (listenMatrix == 1) // Mono Sum (Phase Safe)
        {
            float m = 0.5f * (corrL + corrR);
            outL = m; outR = m;
        }
        else if (listenMatrix == 2) // Side Reveal (L - R envoyé au centre pour entendre la stéréo sur 1 oreille)
        {
            float side = 0.7071f * (corrL - corrR);
            outL = side; outR = side;
        }
        else if (listenMatrix == 3) // Left Only
        {
            outL = corrL; outR = 0.0f;
        }
        else if (listenMatrix == 4) // Right Only
        {
            outL = 0.0f; outR = corrR;
        }

        leftCh[i] = outL;
        if (rightCh != nullptr)
            rightCh[i] = outR;

        pushVisualSample(0.5f * (outL + outR));
    }

    // Mise à jour des indicateurs visuels (Corrélation de phase et RMS L/R)
    float instantCorr = sumLR / std::sqrt(sumLL * sumRR);
    float rmsL = std::sqrt(sumLL / static_cast<float> (std::max(1, numSamples)));
    float rmsR = std::sqrt(sumRR / static_cast<float> (std::max(1, numSamples)));

    const juce::SpinLock::ScopedTryLockType lock(visualLock);
    if (lock.isLocked())
    {
        visCorrelation = 0.85f * visCorrelation + 0.15f * std::clamp(instantCorr, -1.0f, 1.0f);
        visLeftRms = 0.80f * visLeftRms + 0.20f * rmsL;
        visRightRms = 0.80f * visRightRms + 0.20f * rmsR;
        visIsRefPlaying = refPlayingSnap;
    }
}

void SaboteurCortexProcessor::pushVisualSample(float monoOut)
{
    visFifo[visFifoIndex++] = monoOut;
    if (visFifoIndex >= fftSize)
    {
        visFifoIndex = 0;
        computeVisualFft();
    }
}

void SaboteurCortexProcessor::computeVisualFft()
{
    std::fill(fftWorkspace.begin(), fftWorkspace.end(), 0.0f);
    std::copy(visFifo.begin(), visFifo.end(), fftWorkspace.begin());

    visWindow.multiplyWithWindowingTable(fftWorkspace.data(), fftSize);
    visFft.performRealOnlyForwardTransform(fftWorkspace.data());

    const juce::SpinLock::ScopedTryLockType lock(visualLock);
    if (lock.isLocked())
    {
        const float invNorm = 2.0f / static_cast<float> (fftSize);
        for (int i = 1; i <= numBins; ++i)
        {
            float r = fftWorkspace[i * 2];
            float im = fftWorkspace[i * 2 + 1];
            float mag = std::sqrt(r * r + im * im) * invNorm;
            visSpectrum[i] = 0.65f * visSpectrum[i] + 0.35f * mag;
        }
    }
}

float SaboteurCortexProcessor::getCorrectionGainDbAtFreq(float freqHz, bool isLeftChannel) const
{
    if (apvts.getRawParameterValue("eq_bypass")->load() > 0.5f)
        return 0.0f;

    const char* bandIds[numEqBands] = { "b80", "b160", "b315", "b630", "b1250", "b2500", "b5000", "b10000" };
    float totalDb = 0.0f;

    for (int b = 0; b < numEqBands; ++b)
    {
        float gDb = apvts.getRawParameterValue(bandIds[b])->load();
        float octDist = std::log2(std::max(20.0f, freqHz) / bandFreqs[b]);
        totalDb += gDb * std::exp(-2.6f * octDist * octDist);
    }

    if (isLeftChannel)
    {
        float lGain = apvts.getRawParameterValue("left_gain")->load();
        float lShelf = apvts.getRawParameterValue("left_tilt")->load();
        float shelfFactor = 1.0f / (1.0f + std::exp(-3.5f * std::log2(std::max(20.0f, freqHz) / 1600.0f)));
        totalDb += lGain + lShelf * shelfFactor;
    }

    return totalDb;
}

void SaboteurCortexProcessor::getVisualSnapshot(std::vector<float>& outSpectrum,
    float& outPhaseCorrelation,
    float& outLeftRms,
    float& outRightRms,
    bool& outIsRefTonePlaying,
    double& outSampleRate)
{
    const juce::SpinLock::ScopedLockType lock(visualLock);
    outSpectrum = visSpectrum;
    outPhaseCorrelation = visCorrelation;
    outLeftRms = visLeftRms;
    outRightRms = visRightRms;
    outIsRefTonePlaying = visIsRefPlaying;
    outSampleRate = currentSampleRate;
}

// --- MÉTHODES STANDARD JUCE OBLIGATOIRES ---
const juce::String SaboteurCortexProcessor::getName() const { return "Saboteur Cortex"; }
bool SaboteurCortexProcessor::acceptsMidi() const { return false; }
bool SaboteurCortexProcessor::producesMidi() const { return false; }
bool SaboteurCortexProcessor::isMidiEffect() const { return false; }
double SaboteurCortexProcessor::getTailLengthSeconds() const { return 0.0; }

int SaboteurCortexProcessor::getNumPrograms() { return 1; }
int SaboteurCortexProcessor::getCurrentProgram() { return 0; }
void SaboteurCortexProcessor::setCurrentProgram(int) {}
const juce::String SaboteurCortexProcessor::getProgramName(int) { return {}; }
void SaboteurCortexProcessor::changeProgramName(int, const juce::String&) {}

void SaboteurCortexProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SaboteurCortexProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* SaboteurCortexProcessor::createEditor()
{
    return new SaboteurCortexEditor(*this);
}

bool SaboteurCortexProcessor::hasEditor() const { return true; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SaboteurCortexProcessor();
}