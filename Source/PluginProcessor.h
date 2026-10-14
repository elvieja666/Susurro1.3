#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

// Susurro: vocoder de ruido (estilo "whisper" del Morphoder).
// La voz de entrada (modulador) se analiza en N bandas; la envolvente de cada
// banda modula la misma banda de un ruido blanco (portadora). Resultado: un
// susurro que sigue lo que se dice.
class SusurroAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaxBands = 40;

    SusurroAudioProcessor();
    ~SusurroAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Susurro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    // Biquad TDF-II genérico (coeficientes RBJ)
    struct Biquad
    {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        enum Type { BandPass, LowPass, HighPass };
        void set (Type t, double fs, double f0, double q)
        {
            const double w0 = juce::MathConstants<double>::twoPi * f0 / fs;
            const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            double nb0, nb1, nb2;
            switch (t)
            {
                case LowPass:  nb0 = (1 - cw) / 2; nb1 = 1 - cw;    nb2 = (1 - cw) / 2; break;
                case HighPass: nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = (1 + cw) / 2; break;
                case BandPass:
                default:       nb0 = alpha;        nb1 = 0;         nb2 = -alpha;       break;
            }
            b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
            a1 = (float) (-2.0 * cw / a0); a2 = (float) ((1.0 - alpha) / a0);
        }
        void reset() { z1 = z2 = 0; }
        inline float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct Band
    {
        Biquad ana;                             // análisis (modulador)
        std::array<Biquad, 2> syn;              // síntesis por canal (portadora)
        float env = 0;                          // envolvente
        float norm = 1;                         // normalización de nivel del ruido
    };

    struct Rng   // xorshift32: ruido blanco rápido en [-1, 1]
    {
        uint32_t s;
        explicit Rng (uint32_t seed) : s (seed) {}
        inline float next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) ((int32_t) s) * (1.0f / 2147483648.0f);
        }
    };

    void rebuildBands();

    std::array<Band, kMaxBands> bands;
    int numBands = 0;
    float lastFormant = -1.0f;
    float totalEnv = 0.0f;   // envolvente global para Pressure
    int lastBandsParam = -1;
    double fs = 44100.0;

    Rng rngA { 0x12345678u }, rngB { 0x9E3779B9u };
    std::array<juce::dsp::IIR::Filter<float>, 2> airHpf;
    float lastAir = -1.0f;
    std::array<std::array<juce::dsp::IIR::Filter<float>, 5>, 2> eq;
    std::array<float, 5> lastEq {};
    float broadEnv = 0.0f;

    juce::SmoothedValue<float> mixSm, outSm;

    std::atomic<float>* pBands;
    std::atomic<float>* pFormant;
    std::atomic<float>* pAttack;
    std::atomic<float>* pRelease;
    std::atomic<float>* pAir;
    std::atomic<float>* pPressure;
    std::atomic<float>* pWidth;
    std::atomic<float>* pMix;
    std::atomic<float>* pOut;
    std::atomic<float>* pTracking;
    std::array<std::atomic<float>*, 5> pEq {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SusurroAudioProcessor)
};
