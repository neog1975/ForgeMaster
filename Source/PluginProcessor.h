#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Dynamics.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/MixAnalyzer.h"

namespace targets
{
    // Streaming / delivery loudness targets (integrated LUFS). Index 0 = custom.
    inline const juce::StringArray names { "Custom", "Spotify (-14)", "Apple Music (-16)", "YouTube (-14)",
                                           "Amazon Music (-14)", "Tidal (-14)", "Deezer (-15)",
                                           "Broadcast EBU R128 (-23)", "Loud (-9)", "Club / DJ (-8)", "Medium (-11)" };
    inline const float lufs[] { 0.0f, -14.0f, -16.0f, -14.0f, -14.0f, -14.0f, -15.0f, -23.0f, -9.0f, -8.0f, -11.0f };

    // Simple-view loudness buttons: Low / Medium / High
    constexpr int simpleIndex[3] { 1, 10, 8 };
}

class ForgeMasterProcessor : public juce::AudioProcessor,
                             private juce::Timer
{
public:
    static constexpr int numBands = 4;

    ForgeMasterProcessor();
    ~ForgeMasterProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Forge Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getTargetLufs() const;
    void resetIntegrated() { inMeter.requestReset(); outMeter.requestReset(); }

    // One-click auto master: listen to the mix, then derive every setting from it.
    enum class AutoState { idle, listening, mastered };
    void startAutoMaster();
    void cancelAutoMaster() { analyzer.cancel(); autoState.store (AutoState::idle); }
    AutoState getAutoState() const { return autoState.load(); }
    float getAutoProgress() const { return analyzer.getProgress(); }
    juce::String getAutoSummary() const;
    // Message-thread housekeeping (normally driven by the internal timer).
    void updateFromMessageThread();

    juce::AudioProcessorValueTreeState apvts;

    // Meter values published to the editor.
    struct Meters
    {
        std::atomic<float> inShortTerm { -100.0f };
        std::atomic<float> outMomentary { -100.0f }, outShortTerm { -100.0f }, outIntegrated { -100.0f };
        std::atomic<float> truePeakDb { -100.0f };
        std::atomic<float> limiterGrDb { 0.0f };
        std::atomic<float> autoDriveDb { 0.0f };
        std::atomic<float> bandGrDb[numBands] {};
    } meters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void updateFilters();
    void processChunk (float* L, float* R, int n);
    void applyAutoMaster();
    void setParam (const juce::String& id, float value);

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    enum BandParam { bpOn, bpThr, bpRatio, bpAtt, bpRel, bpGain, bpWidth, numBandParams };
    std::atomic<float>* bandParams[numBands][numBandParams] {};
    float band (int b, BandParam p) const { return bandParams[b][p]->load(); }

    double sampleRate = 44100.0;
    int maxBlock = 512;
    int osFactor = 1;
    float truePeakMargin = 0.7f;

    // Tone EQ: low shelf, low-mid bell, presence bell, air shelf
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, 4> eqCoefs;
    std::array<std::array<juce::dsp::IIR::Filter<float>, 2>, 4> eq;
    std::array<float, 8> eqCache {};

    // 4-band Linkwitz-Riley split with allpass phase compensation
    juce::dsp::LinkwitzRileyFilter<float> xo[3], ap1a, ap1b, ap2;
    std::array<BandCompressor, numBands> comps;
    std::vector<float> bandBuf[numBands][2];
    float bandWidth[numBands] { 1.0f, 1.0f, 1.0f, 1.0f };
    float globalWidth = 1.0f;

    juce::dsp::IIR::Filter<float> sideHighPass;
    float monoCache = -1.0f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    LookaheadLimiter limiter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGain, outGain, driveGain;

    LoudnessMeter inMeter, outMeter;
    int lastTicks = 0;
    bool autoWasOn = false;
    float autoDb = 0.0f;
    std::atomic<float> autoDbShared { 0.0f };
    std::atomic<float> autoSeedDb { 0.0f };
    std::atomic<bool> autoSeedPending { false };

    MixAnalyzer analyzer;
    MixAnalyzer::Result analysis;
    bool hasAnalysis = false;
    std::atomic<AutoState> autoState { AutoState::idle };
    int appliedStyle = -1;
    float appliedIntensity = -1.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 16 };
    std::vector<float> dryL, dryR;
    juce::SmoothedValue<float> matchGainDb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeMasterProcessor)
};
