#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Tone offsets in dB for [low shelf, low-mid, presence, air], scaled by intensity.
    constexpr float styleOffsets[4][4] {
        {  1.7f, -0.2f, -0.2f,  1.4f },   // Balanced
        {  2.0f,  0.5f, -0.5f, -1.0f },   // Warm
        { -0.5f, -1.0f,  1.5f,  2.5f },   // Open
        {  0.0f,  0.0f,  0.0f,  0.0f },   // Flat
    };

    constexpr float defaultAttack[4]  { 30.0f, 15.0f, 6.0f, 3.0f };
    constexpr float defaultRelease[4] { 250.0f, 160.0f, 100.0f, 70.0f };

    juce::String bandId (int band, const char* name) { return "b" + juce::String (band + 1) + name; }
}

ForgeMasterProcessor::ForgeMasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    static const char* names[numBandParams] { "On", "Thr", "Ratio", "Att", "Rel", "Gain", "Width" };
    for (int b = 0; b < numBands; ++b)
        for (int p = 0; p < numBandParams; ++p)
            bandParams[b][p] = apvts.getRawParameterValue (bandId (b, names[p]));

    startTimerHz (10);
}

ForgeMasterProcessor::~ForgeMasterProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout ForgeMasterProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, float lo, float hi, float def,
                         const String& label, float skewCentre = 0.0f)
    {
        NormalisableRange<float> range (lo, hi, 0.01f);
        if (skewCentre > 0.0f)
            range.setSkewForCentre (skewCentre);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (label)));
    };

    addFloat ("inGain", "Input Gain", -24.0f, 24.0f, 0.0f, "dB");
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style",
                                                        StringArray { "Balanced", "Warm", "Open", "Flat" }, 0));
    addFloat ("intensity", "Intensity", 0.0f, 100.0f, 50.0f, "%");

    addFloat ("lowGain", "Low Gain", -12.0f, 12.0f, 0.0f, "dB");
    addFloat ("lowFreq", "Low Freq", 20.0f, 300.0f, 100.0f, "Hz", 80.0f);
    addFloat ("lmGain", "Low-Mid Gain", -12.0f, 12.0f, 0.0f, "dB");
    addFloat ("lmFreq", "Low-Mid Freq", 150.0f, 1500.0f, 400.0f, "Hz", 450.0f);
    addFloat ("presGain", "Presence Gain", -12.0f, 12.0f, 0.0f, "dB");
    addFloat ("presFreq", "Presence Freq", 1500.0f, 8000.0f, 3500.0f, "Hz", 3500.0f);
    addFloat ("airGain", "Air Gain", -12.0f, 12.0f, 0.0f, "dB");
    addFloat ("airFreq", "Air Freq", 6000.0f, 18000.0f, 12000.0f, "Hz", 11000.0f);

    addFloat ("xover1", "Crossover Low", 40.0f, 400.0f, 120.0f, "Hz", 120.0f);
    addFloat ("xover2", "Crossover Mid", 400.0f, 4000.0f, 1000.0f, "Hz", 1200.0f);
    addFloat ("xover3", "Crossover High", 2000.0f, 14000.0f, 6000.0f, "Hz", 6000.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "msMode", 1 }, "Mid/Side Dynamics", false));

    for (int b = 0; b < numBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { bandId (b, "On"), 1 }, n + "Comp On", true));
        addFloat (bandId (b, "Thr"), n + "Threshold", -48.0f, 0.0f, -18.0f, "dB");
        addFloat (bandId (b, "Ratio"), n + "Ratio", 1.0f, 10.0f, 2.0f, ":1", 3.0f);
        addFloat (bandId (b, "Att"), n + "Attack", 0.1f, 100.0f, defaultAttack[b], "ms", 10.0f);
        addFloat (bandId (b, "Rel"), n + "Release", 10.0f, 1000.0f, defaultRelease[b], "ms", 150.0f);
        addFloat (bandId (b, "Gain"), n + "Makeup", -12.0f, 12.0f, 0.0f, "dB");
        addFloat (bandId (b, "Width"), n + "Width", 0.0f, 200.0f, 100.0f, "%");
    }

    addFloat ("monoBass", "Mono Bass", 0.0f, 300.0f, 0.0f, "Hz");
    addFloat ("width", "Stereo Width", 0.0f, 200.0f, 100.0f, "%");
    addFloat ("satDrive", "Saturation", 0.0f, 100.0f, 0.0f, "%");
    addFloat ("satMix", "Saturation Mix", 0.0f, 100.0f, 100.0f, "%");

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Loudness Target",
                                                        targets::names, 8));
    addFloat ("customLufs", "Custom Target", -24.0f, -5.0f, -14.0f, "LUFS");
    addFloat ("ceiling", "Ceiling", -3.0f, 0.0f, -1.0f, "dBTP");
    addFloat ("limRelease", "Limiter Release", 1.0f, 500.0f, 60.0f, "ms", 60.0f);
    addFloat ("drive", "Limiter Drive", -12.0f, 24.0f, 0.0f, "dB");
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "autoLoud", 1 }, "Auto Loudness", true));
    addFloat ("outGain", "Output Trim", -12.0f, 12.0f, 0.0f, "dB");
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "compare", 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "gainMatch", 1 }, "Gain Match", false));

    return layout;
}

bool ForgeMasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float ForgeMasterProcessor::getTargetLufs() const
{
    const int idx = (int) param ("target");
    return idx <= 0 ? param ("customLufs") : targets::lufs[idx];
}

void ForgeMasterProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    maxBlock = juce::jmax (1, samplesPerBlock);
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

    for (size_t k = 0; k < eq.size(); ++k)
    {
        eqCoefs[k] = new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0);
        for (auto& f : eq[k]) { f.coefficients = eqCoefs[k]; f.prepare (spec); }
    }
    eqCache.fill (std::numeric_limits<float>::quiet_NaN());

    for (auto& f : xo) { f.setType (juce::dsp::LinkwitzRileyFilterType::lowpass); f.prepare (spec); }
    for (auto* f : { &ap1a, &ap1b, &ap2 }) { f->setType (juce::dsp::LinkwitzRileyFilterType::allpass); f->prepare (spec); }

    for (auto& c : comps) c.prepare (sr);
    for (auto& band : bandBuf)
        for (auto& ch : band) ch.assign ((size_t) maxBlock, 0.0f);

    sideHighPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 20.0);
    sideHighPass.prepare ({ sr, (juce::uint32) maxBlock, 1 });
    monoCache = -1.0f;

    // 4x oversampling at 44.1/48 kHz, 2x at 88.2/96 kHz, none above.
    const int order = sr <= 50000.0 ? 2 : (sr <= 100000.0 ? 1 : 0);
    osFactor = 1 << order;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, (size_t) order, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);

    // ~1.5 ms lookahead, rounded so the limiter delay is a whole number of base-rate samples.
    const int lookBase = juce::jmax (1, (int) std::lround (sr * 0.0015));
    limiter.prepare (lookBase * osFactor + 1);

    const int latency = (int) std::lround (oversampler->getLatencyInSamples()) + lookBase;
    setLatencySamples (latency);

    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) latency);
    dryL.assign ((size_t) maxBlock, 0.0f);
    dryR.assign ((size_t) maxBlock, 0.0f);

    for (auto* s : { &inGain, &outGain, &driveGain })
        s->reset (sr, 0.05);
    inGain.setCurrentAndTargetValue (dbToGain (param ("inGain")));
    outGain.setCurrentAndTargetValue (dbToGain (param ("outGain")));
    driveGain.setCurrentAndTargetValue (dbToGain (param ("drive")));
    matchGainDb.reset (sr, 0.3);
    matchGainDb.setCurrentAndTargetValue (0.0f);

    inMeter.prepare (sr);
    analyzer.prepare (sr);
    outMeter.prepare (sr);
    lastTicks = 0;
    autoWasOn = false;
    autoDb = param ("drive");

    for (auto& w : bandWidth) w = 1.0f;
    globalWidth = param ("width") * 0.01f;
}

void ForgeMasterProcessor::updateFilters()
{
    const int style = (int) param ("style");
    const float amount = param ("intensity") / 50.0f;

    const float gains[4] { param ("lowGain"), param ("lmGain"), param ("presGain"), param ("airGain") };
    const float freqs[4] { param ("lowFreq"), param ("lmFreq"), param ("presFreq"), param ("airFreq") };

    for (int k = 0; k < 4; ++k)
    {
        const float g = gains[k] + styleOffsets[style][k] * amount;
        if (g == eqCache[(size_t) k * 2] && freqs[k] == eqCache[(size_t) k * 2 + 1])
            continue;
        eqCache[(size_t) k * 2] = g;
        eqCache[(size_t) k * 2 + 1] = freqs[k];

        using AC = juce::dsp::IIR::ArrayCoefficients<float>;
        const float lin = dbToGain (g);
        switch (k)
        {
            case 0:  *eqCoefs[0] = AC::makeLowShelf (sampleRate, freqs[k], 0.707f, lin); break;
            case 1:  *eqCoefs[1] = AC::makePeakFilter (sampleRate, freqs[k], 0.9f, lin); break;
            case 2:  *eqCoefs[2] = AC::makePeakFilter (sampleRate, freqs[k], 0.8f, lin); break;
            default: *eqCoefs[3] = AC::makeHighShelf (sampleRate, freqs[k], 0.707f, lin); break;
        }
    }

    // Keep crossovers ordered and at least half an octave apart.
    const float nyq = (float) sampleRate * 0.45f;
    const float f1 = param ("xover1");
    const float f2 = juce::jmax (param ("xover2"), f1 * 1.5f);
    const float f3 = juce::jmin (juce::jmax (param ("xover3"), f2 * 1.5f), nyq);
    xo[0].setCutoffFrequency (f1);
    xo[1].setCutoffFrequency (f2);
    xo[2].setCutoffFrequency (f3);
    ap1a.setCutoffFrequency (f2);
    ap1b.setCutoffFrequency (f3);
    ap2.setCutoffFrequency (f3);

    const float mono = param ("monoBass");
    if (mono != monoCache)
    {
        monoCache = mono;
        *sideHighPass.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (
            sampleRate, juce::jmax (20.0f, mono), 0.707f);
    }

    for (int b = 0; b < numBands; ++b)
        comps[(size_t) b].setTimes (band (b, bpAtt), band (b, bpRel));
}

void ForgeMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();

    updateFilters();

    inGain.setTargetValue (dbToGain (param ("inGain")));
    outGain.setTargetValue (dbToGain (param ("outGain")));

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    for (int start = 0; start < total; start += maxBlock)
        processChunk (L + start, R + start, juce::jmin (maxBlock, total - start));

    for (auto& f : xo) f.snapToZero();
    for (auto* f : { &ap1a, &ap1b, &ap2 }) f->snapToZero();
    for (auto& band : eq)
        for (auto& f : band) f.snapToZero();
    sideHighPass.snapToZero();
}

void ForgeMasterProcessor::processChunk (float* L, float* R, int n)
{
    // Dry copy for the level-matched compare.
    for (int i = 0; i < n; ++i)
    {
        dryDelay.pushSample (0, L[i]); dryDelay.pushSample (1, R[i]);
        dryL[(size_t) i] = dryDelay.popSample (0);
        dryR[(size_t) i] = dryDelay.popSample (1);
    }
    inMeter.process (L, R, n);
    analyzer.process (L, R, n, inMeter.getMomentary() > -40.0f);

    // Input gain + tone EQ
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        float l = L[i] * g, r = R[i] * g;
        for (auto& band : eq)
        {
            l = band[0].processSample (l);
            r = band[1].processSample (r);
        }
        L[i] = l; R[i] = r;
    }

    // Band split
    for (int i = 0; i < n; ++i)
    {
        const float in[2] { L[i], R[i] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float b1, r1, b2, r2, b3, b4;
            xo[0].processSample (ch, in[ch], b1, r1);
            xo[1].processSample (ch, r1, b2, r2);
            xo[2].processSample (ch, r2, b3, b4);
            b1 = ap1b.processSample (ch, ap1a.processSample (ch, b1));
            b2 = ap2.processSample (ch, b2);
            bandBuf[0][ch][(size_t) i] = b1;
            bandBuf[1][ch][(size_t) i] = b2;
            bandBuf[2][ch][(size_t) i] = b3;
            bandBuf[3][ch][(size_t) i] = b4;
        }
    }

    // Per-band dynamics + width, summed back
    const bool msMode = param ("msMode") > 0.5f;
    const float thrShift = -(param ("intensity") - 50.0f) / 50.0f * 6.0f;   // intensity pushes all thresholds
    std::fill (L, L + n, 0.0f);
    std::fill (R, R + n, 0.0f);

    for (int b = 0; b < numBands; ++b)
    {
        float* a = bandBuf[b][0].data();
        float* c = bandBuf[b][1].data();
        const bool on = band (b, bpOn) > 0.5f;
        const float thr = band (b, bpThr) + thrShift;
        const float ratio = band (b, bpRatio);
        const float makeup = band (b, bpGain);
        const float widthTarget = band (b, bpWidth) * 0.01f;

        if (msMode)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (a[i] + c[i]), s = 0.5f * (a[i] - c[i]);
                a[i] = m; c[i] = s;
            }

        float gr = 0.0f;
        if (on)
            gr = comps[(size_t) b].process (a, c, n, thr, ratio, makeup, ! msMode);
        else
            comps[(size_t) b].reset();
        meters.bandGrDb[b].store (gr);

        float& w = bandWidth[b];
        for (int i = 0; i < n; ++i)
        {
            w += (widthTarget - w) * 0.001f;
            float m, s;
            if (msMode) { m = a[i]; s = c[i]; }
            else        { m = 0.5f * (a[i] + c[i]); s = 0.5f * (a[i] - c[i]); }
            s *= w;
            L[i] += m + s;
            R[i] += m - s;
        }
    }

    // Mono bass (side high-pass) + global width
    const bool monoOn = param ("monoBass") >= 20.0f;
    const float widthTarget = param ("width") * 0.01f;
    for (int i = 0; i < n; ++i)
    {
        globalWidth += (widthTarget - globalWidth) * 0.001f;
        const float m = 0.5f * (L[i] + R[i]);
        float s = 0.5f * (L[i] - R[i]);
        if (monoOn) s = sideHighPass.processSample (s);
        s *= globalWidth;
        L[i] = m + s; R[i] = m - s;
    }

    // Loudness drive: manual, or learned by the auto-loudness loop.
    const bool autoOn = param ("autoLoud") > 0.5f;
    if (autoSeedPending.exchange (false))
        autoDb = autoSeedDb.load();
    else if (autoOn && ! autoWasOn)
        autoDb = param ("drive");
    autoWasOn = autoOn;
    driveGain.setTargetValue (dbToGain (autoOn ? autoDb : param ("drive")));

    // Oversampled section: saturation + true-peak limiter
    float* chans[2] { L, R };
    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
    auto os = oversampler->processSamplesUp (block);
    float* oL = os.getChannelPointer (0);
    float* oR = os.getChannelPointer (1);
    const int on = (int) os.getNumSamples();

    const float sat = param ("satDrive") * 0.01f;
    if (sat > 0.0f)
    {
        const float pre = dbToGain (sat * 18.0f);
        const float mix = param ("satMix") * 0.01f;
        for (int i = 0; i < on; ++i)
        {
            oL[i] += mix * (std::tanh (oL[i] * pre) / pre - oL[i]);
            oR[i] += mix * (std::tanh (oR[i] * pre) / pre - oR[i]);
        }
    }

    const float driveStart = driveGain.getCurrentValue();
    driveGain.skip (n);
    const float driveEnd = driveGain.getCurrentValue();
    // The downsampling filter can overshoot the oversampled peak; leave headroom
    // so the final true peak stays under the user's ceiling.
    const float ceiling = dbToGain (param ("ceiling") - truePeakMargin);
    const float relCoef = std::exp (-1.0f / (0.001f * param ("limRelease") * (float) sampleRate * (float) osFactor));
    float peak = 0.0f;
    const float minGain = limiter.process (oL, oR, on, driveStart, driveEnd, ceiling, relCoef, peak);

    oversampler->processSamplesDown (block);

    // Output trim + final safety clip at the ceiling
    const float ceilOut = dbToGain (param ("ceiling"));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        L[i] = juce::jlimit (-ceilOut, ceilOut, L[i] * g);
        R[i] = juce::jlimit (-ceilOut, ceilOut, R[i] * g);
        peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
    }

    outMeter.process (L, R, n);

    const int ticks = outMeter.getTicks();
    if (ticks != lastTicks)
    {
        lastTicks = ticks;
        const float inST = inMeter.getShortTerm();
        const float outST = outMeter.getShortTerm();

        if (autoOn && inMeter.getMomentary() > -45.0f && outST > -70.0f)
        {
            const float step = juce::jlimit (-0.3f, 0.3f, (getTargetLufs() - outST) * 0.05f);
            autoDb = juce::jlimit (-12.0f, 24.0f, autoDb + step);
        }
        if (inST > -70.0f && outST > -70.0f)
            matchGainDb.setTargetValue (juce::jlimit (-24.0f, 24.0f, inST - outST));
    }

    // Bypass plays the untouched mix (latency-compensated). Gain Match turns the
    // master down to the mix's loudness so only the tonal/dynamic change is heard.
    if (param ("compare") > 0.5f)
    {
        std::copy (dryL.begin(), dryL.begin() + n, L);
        std::copy (dryR.begin(), dryR.begin() + n, R);
        matchGainDb.skip (n);
    }
    else if (param ("gainMatch") > 0.5f)
        for (int i = 0; i < n; ++i)
        {
            const float g = dbToGain (matchGainDb.getNextValue());
            L[i] *= g;
            R[i] *= g;
        }
    else
        matchGainDb.skip (n);

    autoDbShared.store (autoDb);
    meters.inShortTerm.store (inMeter.getShortTerm());
    meters.outMomentary.store (outMeter.getMomentary());
    meters.outShortTerm.store (outMeter.getShortTerm());
    meters.outIntegrated.store (outMeter.getIntegrated());
    meters.limiterGrDb.store (gainToDb (minGain));
    meters.autoDriveDb.store (autoOn ? autoDb : param ("drive"));
    const float tp = gainToDb (peak);
    meters.truePeakDb.store (juce::jmax (tp, meters.truePeakDb.load() - 0.05f));
}

void ForgeMasterProcessor::timerCallback() { updateFromMessageThread(); }

void ForgeMasterProcessor::updateFromMessageThread()
{
    if (analyzer.takeResult (analysis))
    {
        hasAnalysis = true;
        appliedStyle = -1;   // force a full apply below
        autoState.store (AutoState::mastered);
    }

    // Re-derive the master whenever style or intensity changes, like a one-knob mastering engine.
    if (hasAnalysis && autoState.load() != AutoState::listening
        && ((int) param ("style") != appliedStyle || std::abs (param ("intensity") - appliedIntensity) > 0.5f))
    {
        applyAutoMaster();
        autoState.store (AutoState::mastered);
    }

    // While auto loudness is learning, mirror the learned value onto the Drive
    // parameter so it is saved with the session and stays put once auto is off.
    if (param ("autoLoud") < 0.5f)
        return;
    const float learned = autoDbShared.load();
    auto* drive = apvts.getParameter ("drive");
    if (std::abs (drive->convertFrom0to1 (drive->getValue()) - learned) > 0.05f)
        drive->setValueNotifyingHost (drive->convertTo0to1 (learned));
}

void ForgeMasterProcessor::startAutoMaster()
{
    autoState.store (AutoState::listening);
    analyzer.start();
}

void ForgeMasterProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void ForgeMasterProcessor::applyAutoMaster()
{
    const auto& a = analysis;
    const int style = (int) param ("style");
    const float intensity = param ("intensity") / 100.0f;
    appliedStyle = style;
    appliedIntensity = param ("intensity");

    // --- Tone: compare the octave spectrum against a reference curve, aligned on
    // the 315 Hz - 2.5 kHz region. The curve was measured from a finished,
    // commercially mastered EDM track (loudest sections, same filterbank).
    constexpr float reference[MixAnalyzer::numBands] { 7.0f, 9.0f, 6.0f, 1.5f, 0.0f, -0.5f, -1.0f, -2.5f, -5.0f, -9.5f };
    float offset = 0.0f;
    for (int k = 3; k <= 6; ++k)
        offset += (a.bandDb[(size_t) k] - reference[k]) / 4.0f;

    auto correction = [&] (std::initializer_list<int> ks)
    {
        float dev = 0.0f;
        for (int k : ks) dev += a.bandDb[(size_t) k] - offset - reference[k];
        dev /= (float) ks.size();
        // Only part of the deviation is corrected, and capped: a master should
        // polish the mix's tonal balance, not rebuild it.
        return juce::jlimit (-2.5f, 2.5f, -dev * (0.3f + 0.3f * intensity));
    };
    const float eqGain[4] { correction ({ 0, 1, 2 }), correction ({ 3, 4 }), correction ({ 6, 7 }), correction ({ 8, 9 }) };
    setParam ("lowGain", eqGain[0]);
    setParam ("lowFreq", 130.0f);
    setParam ("lmGain", eqGain[1]);
    setParam ("lmFreq", 400.0f);
    setParam ("presGain", eqGain[2]);
    setParam ("presFreq", 3500.0f);
    setParam ("airGain", eqGain[3]);
    setParam ("airFreq", 12000.0f);

    // Total tone change the compressor bands will see (EQ + style tilt), so the
    // dynamics don't pull boosted bands straight back down.
    float toneDb[4];
    for (int k = 0; k < 4; ++k)
        toneDb[k] = eqGain[k] + styleOffsets[style][k] * appliedIntensity / 50.0f;
    const float bandToneDb[numBands] { toneDb[0], 0.5f * toneDb[1], toneDb[2], toneDb[3] };

    // --- Dynamics: thresholds sit around each band's RMS (higher at low intensity,
    // below it at high intensity); the intensity macro in the DSP adds to this.
    setParam ("xover1", 120.0f);
    setParam ("xover2", 1000.0f);
    setParam ("xover3", 6000.0f);
    constexpr int bandMap[numBands][3] { { 0, 1, -1 }, { 2, 3, 4 }, { 5, 6, 7 }, { 8, 9, -1 } };
    const bool alreadySquashed = a.crestDb < 9.0f;
    const bool veryDynamic = a.crestDb > 15.0f;
    for (int b = 0; b < numBands; ++b)
    {
        double energy = 0.0;
        for (int k : bandMap[b])
            if (k >= 0) energy += std::pow (10.0, a.bandDb[(size_t) k] / 10.0);
        const float rmsDb = (float) (10.0 * std::log10 (energy + 1e-20));

        // High bands are peaky (hats, cymbals): a peak detector would dull them,
        // so they get a higher threshold and a gentler ratio.
        constexpr float bandThrOffset[numBands] { 0.0f, 0.0f, 3.0f, 5.0f };
        constexpr float bandRatioScale[numBands] { 1.0f, 1.0f, 0.7f, 0.5f };
        float ratio = 1.5f + 1.0f * intensity;
        if (alreadySquashed) ratio = juce::jmax (1.2f, ratio - 0.7f);
        if (veryDynamic) ratio += 0.5f;
        ratio = 1.0f + (ratio - 1.0f) * bandRatioScale[b];

        const juce::String pre = "b" + juce::String (b + 1);
        setParam (pre + "On", 1.0f);
        setParam (pre + "Thr", juce::jlimit (-48.0f, 0.0f, rmsDb + bandToneDb[b] + bandThrOffset[b] + 10.0f - 8.0f * intensity));
        setParam (pre + "Ratio", ratio);
        setParam (pre + "Gain", 0.0f);
    }
    setParam ("limRelease", veryDynamic ? 100.0f : 50.0f);

    // --- Stereo: mono the sub if it carries side energy, adjust width by correlation.
    setParam ("monoBass", a.lowSideDb > -12.0f ? 60.0f : 0.0f);   // only for clearly wide sub
    float width = a.correlation > 0.9f ? 110.0f : (a.correlation < 0.3f ? 95.0f : 100.0f);
    width += style == 2 ? 8.0f : (style == 1 ? -4.0f : 0.0f);
    setParam ("width", width);
    for (int b = 0; b < numBands; ++b)
        setParam ("b" + juce::String (b + 1) + "Width", b == 3 && style == 2 ? 110.0f : 100.0f);

    // --- Colour: saturation by style.
    constexpr float satByStyle[4] { 15.0f, 25.0f, 10.0f, 0.0f };
    setParam ("satDrive", satByStyle[style] * (0.5f + intensity));
    setParam ("satMix", 100.0f);

    // --- Loudness: seed the limiter drive from the measured loudness; the
    // auto-loudness loop then fine-tunes it while the track plays.
    const float seed = a.integratedLufs > -70.0f
                         ? juce::jlimit (-12.0f, 24.0f, getTargetLufs() - (a.integratedLufs + param ("inGain")))
                         : param ("drive");
    setParam ("drive", seed);
    setParam ("autoLoud", 1.0f);
    autoSeedDb.store (seed);
    autoSeedPending.store (true);
}

juce::String ForgeMasterProcessor::getAutoSummary() const
{
    if (! hasAnalysis)
        return {};
    const auto& a = analysis;
    return "Mix: " + juce::String (a.integratedLufs, 1) + " LUFS, crest " + juce::String (a.crestDb, 1)
         + " dB, correlation " + juce::String (a.correlation, 2)
         + (a.lowSideDb > -25.0f ? ", wide low end (mono bass on)" : "");
}

void ForgeMasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (hasAnalysis)
    {
        juce::ValueTree t ("Analysis");
        juce::StringArray bands;
        for (float v : analysis.bandDb) bands.add (juce::String (v));
        t.setProperty ("bands", bands.joinIntoString (","), nullptr)
         .setProperty ("crest", analysis.crestDb, nullptr)
         .setProperty ("corr", analysis.correlation, nullptr)
         .setProperty ("lowSide", analysis.lowSideDb, nullptr)
         .setProperty ("lufs", analysis.integratedLufs, nullptr)
         .setProperty ("style", appliedStyle, nullptr)
         .setProperty ("intensity", appliedIntensity, nullptr);
        state.appendChild (t, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ForgeMasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto t = state.getChildWithName ("Analysis");
    if (t.isValid())
    {
        // Restore the analysis without re-applying it, so saved tweaks survive.
        auto bands = juce::StringArray::fromTokens (t["bands"].toString(), ",", "");
        for (int k = 0; k < juce::jmin (bands.size(), MixAnalyzer::numBands); ++k)
            analysis.bandDb[(size_t) k] = bands[k].getFloatValue();
        analysis.crestDb = t["crest"];
        analysis.correlation = t["corr"];
        analysis.lowSideDb = t["lowSide"];
        analysis.integratedLufs = t["lufs"];
        appliedStyle = t["style"];
        appliedIntensity = t["intensity"];
        hasAnalysis = true;
        autoState.store (AutoState::mastered);
        state.removeChild (t, nullptr);
    }
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* ForgeMasterProcessor::createEditor() { return new ForgeMasterEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ForgeMasterProcessor(); }
