// Offline host: loads the built Forge Master VST3, runs a WAV through it twice
// (first pass lets Auto Loudness learn, second pass is rendered) and writes the result.
// Usage: ForgeRender <plugin.vst3> <in.wav> <out.wav> [param=value ...]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3 || (argc < 4 && juce::String (argv[2]) != "--list"))
    {
        std::cerr << "usage: ForgeRender <plugin.vst3> (--list | <in.wav> <out.wav> [\"Param Name=value\" ...])\n";
        return 1;
    }

    juce::AudioPluginFormatManager formats;
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, argv[1]);
    if (found.isEmpty()) { std::cerr << "plugin not found\n"; return 1; }

    // --list: print every parameter the plugin exposes to hosts (name, current value, options).
    if (juce::String (argv[2]) == "--list")
    {
        juce::String error;
        auto inst = formats.createPluginInstance (*found[0], 48000.0, 512, error);
        if (inst == nullptr) { std::cerr << error << "\n"; return 1; }
        std::cout << inst->getName() << ": " << inst->getParameters().size() << " parameters, latency "
                  << inst->getLatencySamples() << "\n";
        for (auto* p : inst->getParameters())
        {
            std::cout << "  " << p->getName (64) << " = " << p->getCurrentValueAsText();
            if (p->getNumSteps() > 1 && p->getNumSteps() <= 16)
            {
                std::cout << "  [";
                for (int i = 0; i < p->getNumSteps(); ++i)
                    std::cout << (i ? " | " : "") << p->getText ((float) i / (float) (p->getNumSteps() - 1), 64);
                std::cout << "]";
            }
            std::cout << "\n";
        }
        return 0;
    }

    juce::AudioFormatManager audioFormats;
    audioFormats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (audioFormats.createReaderFor (juce::File (argv[2])));
    if (reader == nullptr) { std::cerr << "cannot read input\n"; return 1; }

    const double sr = reader->sampleRate;
    const int len = (int) reader->lengthInSamples;
    constexpr int block = 512;

    juce::String error;
    auto plugin = formats.createPluginInstance (*found[0], sr, block, error);
    if (plugin == nullptr) { std::cerr << error << "\n"; return 1; }

    for (int a = 4; a < argc; ++a)
    {
        auto kv = juce::StringArray::fromTokens (argv[a], "=", "");
        for (auto* p : plugin->getParameters())
            if (p->getName (64) == kv[0])
                {
                    p->setValue (p->getValueForText (kv[1]));
                    std::cout << "set " << kv[0] << " = " << p->getCurrentValueAsText() << "\n";
                }
    }

    juce::AudioBuffer<float> input (2, len);
    reader->read (&input, 0, len, 0, true, true);

    plugin->setPlayConfigDetails (2, 2, sr, block);
    plugin->prepareToPlay (sr, block);
    const int latency = plugin->getLatencySamples();
    std::cout << "latency " << latency << " samples\n";

    juce::AudioBuffer<float> output (2, len + latency);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int pos = 0; pos < len + latency; pos += block)
        {
            const int n = juce::jmin (block, len + latency - pos);
            buf.setSize (2, n, false, false, true);
            buf.clear();
            for (int ch = 0; ch < 2; ++ch)
                if (pos < len)
                    buf.copyFrom (ch, 0, input, ch, pos, juce::jmin (n, len - pos));
            plugin->processBlock (buf, midi);
            if (pass == 1)
                for (int ch = 0; ch < 2; ++ch)
                    output.copyFrom (ch, pos, buf, ch, 0, n);
        }
        std::cout << "pass " << pass + 1 << " done\n";
    }
    plugin->releaseResources();

    juce::File outFile (argv[3]);
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (
        new juce::FileOutputStream (outFile), sr, 2, 24, {}, 0));
    writer->writeFromAudioSampleBuffer (output, latency, len);   // drop latency so output aligns with input
    std::cout << "wrote " << outFile.getFullPathName() << "\n";
    return 0;
}
