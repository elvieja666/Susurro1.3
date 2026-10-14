// Prueba offline: pasa un WAV por SusurroAudioProcessor y escribe el resultado.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) return 1;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (argv[1])));
    if (! r) return 2;
    const int n = (int) r->lengthInSamples;
    juce::AudioBuffer<float> buf (2, n);
    r->read (&buf, 0, n, 0, true, true);
    SusurroAudioProcessor p;
    for (int a = 3; a < argc; ++a) { juce::String kv (argv[a]); auto* prm = p.apvts.getParameter (kv.upToFirstOccurrenceOf ("=", false, false));
        if (prm) prm->setValueNotifyingHost (prm->convertTo0to1 (kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue())); }
    p.setPlayConfigDetails (2, 2, r->sampleRate, 512);
    p.prepareToPlay (r->sampleRate, 512);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += 512)
    {
        const int len = juce::jmin (512, n - pos);
        juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 2, pos, len);
        p.processBlock (blk, midi);
    }
    juce::File (argv[2]).deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (juce::File (argv[2])),
                                                r->sampleRate, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (buf, 0, n);
    return 0;
}
