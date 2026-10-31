// Runs the processor directly: one-click auto master on a WAV, then renders
// the result. Usage: AutoMasterTest <in.wav> <out.wav> [style 0-3] [intensity 0-100] [target index] [analyse-from seconds]
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    // --snapshot <out.png> [advanced]: render the editor to an image (UI check without a DAW)
    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
    {
        ForgeMasterProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        if (argc > 3)
            if (auto* tab = dynamic_cast<juce::Button*> (ed->findChildWithID ("advancedTab")))
                tab->onClick();
        juce::File out (argv[2]);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat().writeImageToStream (ed->createComponentSnapshot (ed->getLocalBounds()), stream);
        return 0;
    }

    if (argc < 3) { std::cerr << "usage: AutoMasterTest <in.wav> <out.wav> [style] [intensity] [target]\n"; return 1; }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[1])));
    if (! reader) { std::cerr << "cannot read input\n"; return 1; }
    const double sr = reader->sampleRate;
    const int len = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> input (2, len);
    reader->read (&input, 0, len, 0, true, true);

    ForgeMasterProcessor proc;
    auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    if (argc > 3) set ("style", (float) std::atoi (argv[3]));
    if (argc > 4) set ("intensity", (float) std::atof (argv[4]));
    if (argc > 5)
    {
        const float t = (float) std::atof (argv[5]);
        if (t < 0.0f) { set ("target", 0.0f); set ("customLufs", t); }   // negative = custom LUFS
        else set ("target", t);
    }

    // extra "paramId=value" overrides; a leading '@' applies them after the auto master ran
    auto applyOverrides = [&] (bool afterAuto)
    {
        for (int a = 7; a < argc; ++a)
        {
            juce::String arg (argv[a]);
            if (arg.startsWithChar ('@') != afterAuto) continue;
            auto kv = juce::StringArray::fromTokens (arg.trimCharactersAtStart ("@"), "=", "");
            if (kv.size() == 2 && proc.apvts.getParameter (kv[0]) != nullptr)
                set (kv[0].toRawUTF8(), kv[1].getFloatValue());
        }
    };
    applyOverrides (false);

    constexpr int block = 512;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    const int latency = proc.getLatencySamples();

    juce::AudioBuffer<float> output (2, len + latency), buf (2, block);
    juce::MidiBuffer midi;
    const int analyseFrom = argc > 6 ? (int) (std::atof (argv[6]) * sr) : 0;

    for (int pass = 0; pass < 2; ++pass)
    {
        if (pass == 1) { applyOverrides (true); proc.resetIntegrated(); }
        for (int pos = 0; pos < len + latency; pos += block)
        {
            const int n = juce::jmin (block, len + latency - pos);
            buf.setSize (2, n, false, false, true);
            buf.clear();
            for (int ch = 0; ch < 2; ++ch)
                if (pos < len) buf.copyFrom (ch, 0, input, ch, pos, juce::jmin (n, len - pos));
            if (pass == 0 && pos <= analyseFrom && analyseFrom < pos + block)
                proc.startAutoMaster();
            proc.processBlock (buf, midi);
            if (pos % (block * 8) == 0) proc.updateFromMessageThread();   // ~10 Hz like the real timer
            if (pass == 1)
                for (int ch = 0; ch < 2; ++ch) output.copyFrom (ch, pos, buf, ch, 0, n);
        }
    }

    std::cout << proc.getAutoSummary() << "\n";
    for (auto* id : { "style", "intensity", "lowGain", "lmGain", "presGain", "airGain", "b1Thr", "b2Thr", "b3Thr", "b4Thr",
                      "b1Ratio", "monoBass", "width", "satDrive", "drive", "limRelease" })
        std::cout << "  " << id << " = " << proc.apvts.getParameter (id)->getCurrentValueAsText() << "\n";
    std::cout << "  plugin integrated = " << proc.meters.outIntegrated.load() << " LUFS (target " << proc.getTargetLufs() << ")\n";

    juce::File out (argv[2]);
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (output, latency, len);
    return 0;
}
