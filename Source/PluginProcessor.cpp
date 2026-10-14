#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Mismas frecuencias extremas que el preset Whisper del Morphoder:
    // con 5 bandas quedan ~258 / 660 / 1690 / 4320 / 11046 Hz
    constexpr double kLowHz  = 258.0;
    constexpr double kHighHz = 11046.0;
    constexpr float  kPressureRef = 0.16f;   // nivel de referencia de la compresión (calibrado: Pressure no cambia el volumen)
    constexpr float  kMaxBoost    = 16.0f;   // límite de realce (+24 dB) para no inflar el silencio
    constexpr float  kBroadRef    = 0.06f;   // referencia de Pressure para la envolvente global
    constexpr float  kNoiseGain   = 2.4f;    // calibración: ruido ~ mismo nivel que la voz (EQ plana, sin HPF)

    // EQ del ruido: mismas frecuencias que el preset Whisper del Morphoder
    constexpr std::array<float, 5> kEqFreq { 258.0f, 689.0f, 1809.0f, 4436.0f, 11025.0f };
    constexpr std::array<float, 5> kEqDefault { 0.0f, 0.0f, 0.0f, 14.0f, 0.0f };
    const char* const kEqIds[5] { "eq1", "eq2", "eq3", "eq4", "eq5" };
    const char* const kEqNames[5] { "EQ 258 Hz", "EQ 689 Hz", "EQ 1.8 kHz", "EQ 4.4 kHz", "EQ 11 kHz" };
}

juce::AudioProcessorValueTreeState::ParameterLayout SusurroAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "bands", 1 }, "Bands", 3, kMaxBands, 5));

    NormalisableRange<float> formant (0.5f, 2.0f, 0.01f);
    formant.setSkewForCentre (1.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "formant", 1 }, "Formant", formant, 1.0f));

    NormalisableRange<float> attack (0.5f, 300.0f, 0.1f);
    attack.setSkewForCentre (40.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Smoothing", attack, 10.0f,
                    AudioParameterFloatAttributes().withLabel ("ms")));

    NormalisableRange<float> release (10.0f, 500.0f, 1.0f);
    release.setSkewForCentre (80.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", release, 120.0f,
                    AudioParameterFloatAttributes().withLabel ("ms")));

    NormalisableRange<float> air (20.0f, 4000.0f, 1.0f);
    air.setSkewForCentre (300.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "air", 1 }, "Air", air, 620.0f,
                    AudioParameterFloatAttributes().withLabel ("Hz")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pressure", 1 }, "Pressure",
                    NormalisableRange<float> (0.0f, 100.0f, 1.0f), 20.0f,
                    AudioParameterFloatAttributes().withLabel ("%")));

    // Tracking: 0% = Whisper del Morphoder (ruido de timbre fijo que sigue el volumen de la voz)
    //           100% = vocoder por bandas (el ruido sigue también el timbre de la voz)
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tracking", 1 }, "Tracking",
                    NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                    AudioParameterFloatAttributes().withLabel ("%")));

    for (size_t i = 0; i < 5; ++i)
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { kEqIds[i], 1 }, kEqNames[i],
                        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), kEqDefault[i],
                        AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width",
                    NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f,
                    AudioParameterFloatAttributes().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                    NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                    AudioParameterFloatAttributes().withLabel ("%")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), -10.0f,
                    AudioParameterFloatAttributes().withLabel ("dB")));

    return { p.begin(), p.end() };
}

SusurroAudioProcessor::SusurroAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Susurro", createLayout())
{
    pBands    = apvts.getRawParameterValue ("bands");
    pFormant  = apvts.getRawParameterValue ("formant");
    pAttack   = apvts.getRawParameterValue ("attack");
    pRelease  = apvts.getRawParameterValue ("release");
    pAir      = apvts.getRawParameterValue ("air");
    pPressure = apvts.getRawParameterValue ("pressure");
    pWidth    = apvts.getRawParameterValue ("width");
    pMix      = apvts.getRawParameterValue ("mix");
    pOut      = apvts.getRawParameterValue ("output");
    pTracking = apvts.getRawParameterValue ("tracking");
    for (size_t i = 0; i < 5; ++i)
        pEq[i] = apvts.getRawParameterValue (kEqIds[i]);
}

bool SusurroAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SusurroAudioProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    lastBandsParam = -1;
    lastFormant = -1.0f;
    lastAir = -1.0f;
    for (auto& f : airHpf) f.reset();
    for (auto& ch : eq) for (auto& f : ch) f.reset();
    lastEq.fill (-999.0f);
    broadEnv = 0.0f;
    mixSm.reset (sampleRate, 0.03);
    outSm.reset (sampleRate, 0.03);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    rebuildBands();
}

// Reparte las bandas en escala logarítmica y calcula filtros + normalización.
void SusurroAudioProcessor::rebuildBands()
{
    const int n = juce::jlimit (3, kMaxBands, (int) pBands->load());
    const float formant = pFormant->load();
    const bool countChanged = (n != numBands);

    numBands = n;
    lastBandsParam = n;
    lastFormant = formant;

    const double ratio = std::pow (kHighHz / kLowHz, 1.0 / (n - 1));
    const double bwOct = std::log2 (ratio);
    const double qAuto = std::sqrt (std::pow (2.0, bwOct)) / (std::pow (2.0, bwOct) - 1.0);
    const double nyqSafe = fs * 0.45;

    // Con 5 bandas: layout exacto del preset Whisper del Morphoder
    //   258 Hz pasabajos Q2 | 689 / 1809 / 4436 Hz pasabanda Q1 | 11025 Hz pasaaltos Q2
    static constexpr double kPresetF[5] { 258.0, 689.0, 1809.0, 4436.0, 11025.0 };
    static constexpr double kPresetQ[5] { 2.0, 1.0, 1.0, 1.0, 2.0 };

    juce::Random rnd (1234);
    for (int i = 0; i < n; ++i)
    {
        auto& b = bands[(size_t) i];
        double fa, q;
        Biquad::Type type = Biquad::BandPass;
        if (n == 5) { fa = kPresetF[i]; q = kPresetQ[i]; }
        else        { fa = kLowHz * std::pow (ratio, i); q = qAuto; }
        if (i == 0)     type = Biquad::LowPass;
        if (i == n - 1) type = Biquad::HighPass;

        const double fan  = juce::jmin (fa, nyqSafe);
        const double fsyn = juce::jmin (fa * formant, nyqSafe);
        b.ana.set (type, fs, fan, q);
        for (auto& s : b.syn)
            s.set (type, fs, fsyn, q);

        // Normalización: nivel RMS real del ruido blanco a través del filtro de síntesis
        Biquad probe = b.syn[0];
        probe.reset();
        double acc = 0.0;
        for (int k = 0; k < 6144; ++k)
        {
            const float y = probe.process (rnd.nextFloat() * 2.0f - 1.0f);
            if (k >= 2048) acc += (double) y * y;
        }
        const double rms = std::sqrt (acc / 4096.0);
        b.norm = (float) (1.0 / juce::jmax (rms, 1e-6));

        if (countChanged)
        {
            b.ana.reset(); b.env = 0;
            for (auto& s : b.syn) s.reset();
        }
    }
}

void SusurroAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int c = numIn; c < numOut; ++c)
        buffer.clear (c, 0, numSamples);

    // Parámetros que reconstruyen filtros
    if ((int) pBands->load() != lastBandsParam || std::abs (pFormant->load() - lastFormant) > 1e-4f)
        rebuildBands();

    const float air = pAir->load();
    if (std::abs (air - lastAir) > 0.5f)
    {
        auto coefs = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, air, 0.707f);
        for (auto& f : airHpf) f.coefficients = coefs;
        lastAir = air;
    }

    // EQ del ruido (shelf grave, 3 campanas, shelf agudo)
    for (size_t i = 0; i < 5; ++i)
    {
        const float g = pEq[i]->load();
        if (std::abs (g - lastEq[i]) < 0.01f) continue;
        lastEq[i] = g;
        const float gain = juce::Decibels::decibelsToGain (g);
        const float f = (float) juce::jmin ((double) kEqFreq[i], fs * 0.45);
        juce::dsp::IIR::Coefficients<float>::Ptr coefs;
        if (i == 0)      coefs = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (fs, f, 0.707f, gain);
        else if (i == 4) coefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, f, 0.707f, gain);
        else             coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, f, 1.0f, gain);
        for (auto& ch : eq) ch[i].coefficients = coefs;
    }

    const float tracking = pTracking->load() * 0.01f;

    const float atk = std::exp (-1.0f / (pAttack->load() * 0.001f * (float) fs));
    const float rel = std::exp (-1.0f / (pRelease->load() * 0.001f * (float) fs));
    const float pressure = pPressure->load() * 0.01f;   // 0..1
    const float widthAngle = pWidth->load() * 0.01f * juce::MathConstants<float>::halfPi;
    const float wCos = std::cos (widthAngle), wSin = std::sin (widthAngle);
    mixSm.setTargetValue (pMix->load() * 0.01f);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool stereoOut = outR != nullptr;

    for (int s = 0; s < numSamples; ++s)
    {
        const float dryL = inL[s];
        const float dryR = inR[s];
        const float mod = 0.5f * (dryL + dryR);

        // Portadora: ruido blanco, con decorrelación L/R según Width
        const float nA = rngA.next();
        const float nB = rngB.next();
        const float noiseL = nA;
        const float noiseR = nA * wCos + nB * wSin;

        float wetL = 0, wetR = 0;

        // A) Whisper del Morphoder: ruido de timbre fijo cuyo volumen sigue a la voz
        if (tracking < 0.999f)
        {
            const float rect = std::abs (mod);
            const float coef = rect > broadEnv ? atk : rel;
            broadEnv = coef * broadEnv + (1.0f - coef) * rect;
            float press = 1.0f;
            if (pressure > 0.001f)   // Pressure: 0% sigue la dinámica, 100% volumen constante
                press = juce::jmin (kMaxBoost, std::pow (juce::jmax (broadEnv, 1e-6f) / kBroadRef, -pressure));
            const float g = broadEnv * press * kNoiseGain * (1.0f - tracking);
            wetL += g * noiseL;
            wetR += g * noiseR;
        }

        // B) Vocoder por bandas: el ruido sigue también el timbre de la voz
        if (tracking > 0.001f)
        {
            float envSum = 0.0f;
            for (int i = 0; i < numBands; ++i)
            {
                auto& b = bands[(size_t) i];
                const float rect = std::abs (b.ana.process (mod));
                const float coef = rect > b.env ? atk : rel;
                b.env = coef * b.env + (1.0f - coef) * rect;
                envSum += b.env;
            }
            float press = 1.0f;
            if (pressure > 0.001f)
                press = juce::jmin (kMaxBoost, std::pow (juce::jmax (envSum, 1e-6f) / kPressureRef, -pressure));
            for (int i = 0; i < numBands; ++i)
            {
                auto& b = bands[(size_t) i];
                const float g = b.env * b.norm * press * tracking;
                wetL += g * b.syn[0].process (noiseL);
                if (stereoOut)
                    wetR += g * b.syn[1].process (noiseR);
            }
        }

        // Filtro del ruido (Noise Filter/Freq) + EQ de 5 bandas
        wetL = airHpf[0].processSample (wetL);
        for (auto& f : eq[0]) wetL = f.processSample (wetL);
        if (stereoOut)
        {
            wetR = airHpf[1].processSample (wetR);
            for (auto& f : eq[1]) wetR = f.processSample (wetR);
        }

        const float mix = mixSm.getNextValue();
        const float gain = outSm.getNextValue();
        outL[s] = (dryL * (1.0f - mix) + wetL * mix) * gain;
        if (stereoOut)
            outR[s] = (dryR * (1.0f - mix) + wetR * mix) * gain;
    }
}

juce::AudioProcessorEditor* SusurroAudioProcessor::createEditor()
{
    return new SusurroAudioProcessorEditor (*this);
}

void SusurroAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SusurroAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SusurroAudioProcessor();
}
