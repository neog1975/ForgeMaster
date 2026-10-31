#include "PluginEditor.h"

// Row height for knobs in the advanced view (caption + dial + value, with breathing room).
constexpr int advancedKnobHeight = 104;

namespace colours
{
    const juce::Colour bg       { 0xff121418 };
    const juce::Colour panel    { 0xff1b1e24 };
    const juce::Colour outline  { 0xff2a2e36 };
    const juce::Colour text     { 0xffd8dce4 };
    const juce::Colour dim      { 0xff8a919e };
    const juce::Colour accent   { 0xffff8a3d };
    const juce::Colour accent2  { 0xff4fc3f7 };
    const juce::Colour good     { 0xff7ed957 };
    const juce::Colour warn     { 0xffff5252 };
}

//==============================================================================
ForgeLookAndFeel::ForgeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::dim);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ComboBox::backgroundColourId, colours::bg);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.3f));
    setColour (juce::TextButton::buttonColourId, colours::bg);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void ForgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float stroke = 3.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::outline);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, angle, true);
    g.setColour (colours::accent);
    g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - stroke * 2.5f;
    g.setColour (colours::panel.brighter (0.15f));
    g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -inner + 3.0f, 3.0f, inner * 0.5f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& caption)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setNumDecimalPlacesToDisplay (1);
    slider.setDoubleClickReturnValue (true, (double) state.getParameter (id)->convertFrom0to1 (
                                                state.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);

    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds().withTrimmedTop (8);   // gap from the value of the row above
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

//==============================================================================
void AutoMasterButton::paint (juce::Graphics& g)
{
    using State = ForgeMasterProcessor::AutoState;
    const auto b = getLocalBounds().toFloat().reduced (8.0f);
    const float size = juce::jmin (b.getWidth(), b.getHeight());
    const auto circle = b.withSizeKeepingCentre (size, size);
    const bool hover = isMouseOver();

    g.setColour (colours::bg);
    g.fillEllipse (circle);

    // progress / done ring
    juce::Path ring;
    const float ringFrac = state == State::listening ? progress : (state == State::mastered ? 1.0f : 0.0f);
    g.setColour (colours::outline);
    g.drawEllipse (circle.reduced (6.0f), 8.0f);
    if (ringFrac > 0.0f)
    {
        ring.addCentredArc (circle.getCentreX(), circle.getCentreY(), size * 0.5f - 6.0f, size * 0.5f - 6.0f, 0.0f,
                            0.0f, juce::MathConstants<float>::twoPi * ringFrac, true);
        g.setColour (state == State::mastered ? colours::good : colours::accent);
        g.strokePath (ring, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto inner = circle.reduced (22.0f);
    g.setGradientFill (juce::ColourGradient (colours::accent.brighter (hover ? 0.3f : 0.1f), inner.getCentreX(), inner.getY(),
                                             colours::accent.darker (0.6f), inner.getCentreX(), inner.getBottom(), false));
    g.fillEllipse (inner);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (size * 0.085f, juce::Font::bold));
    const juce::String title = state == State::listening ? "LISTENING " + juce::String (juce::roundToInt (progress * 100.0f)) + "%"
                             : state == State::mastered  ? "RE-MASTER"
                                                         : "AUTO MASTER";
    g.drawText (title, inner.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
ForgeMasterEditor::ForgeMasterEditor (ForgeMasterProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    knob ("inGain", "Input");
    knob ("intensity", "Intensity");
    combo ("style", { "Balanced", "Warm", "Open", "Flat" });
    knob ("lowGain", "Low");        knob ("lowFreq", "Low Hz");
    knob ("lmGain", "Low-Mid");     knob ("lmFreq", "Low-Mid Hz");
    knob ("presGain", "Presence");  knob ("presFreq", "Presence Hz");
    knob ("airGain", "Air");        knob ("airFreq", "Air Hz");

    knob ("xover1", "Low | Mid");
    knob ("xover2", "Mid | Hi-Mid");
    knob ("xover3", "Hi-Mid | High");
    toggle ("msMode", "Mid/Side");

    for (int b = 0; b < ForgeMasterProcessor::numBands; ++b)
    {
        const juce::String pre = "b" + juce::String (b + 1);
        toggle (pre + "On", "Comp");
        knob (pre + "Thr", "Threshold");
        knob (pre + "Ratio", "Ratio");
        knob (pre + "Att", "Attack");
        knob (pre + "Rel", "Release");
        knob (pre + "Gain", "Makeup");
        knob (pre + "Width", "Width");
    }

    knob ("width", "Width");
    knob ("monoBass", "Mono Bass");
    knob ("satDrive", "Saturation");
    knob ("satMix", "Sat Mix");
    knob ("drive", "Drive");
    knob ("ceiling", "Ceiling");
    knob ("limRelease", "Release");
    knob ("customLufs", "Custom LUFS");
    knob ("outGain", "Output");
    combo ("target", targets::names);
    toggle ("autoLoud", "Auto Loudness");
    toggle ("compare", "Bypass");
    toggle ("gainMatch", "Gain Match");

    resetButton.onClick = [this] { proc.resetIntegrated(); };
    addAndMakeVisible (resetButton);

    // Everything created so far, except the loudness controls, belongs to the advanced view.
    for (auto& [id, k] : knobs) advancedOnly.push_back (k.get());
    for (auto& [id, t] : toggles)
        if (id != "autoLoud" && id != "compare" && id != "gainMatch") advancedOnly.push_back (t.get());
    advancedOnly.push_back (combos["style"].get());

    // --- Simple view
    for (auto* tab : { &simpleTab, &advancedTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1);
        tab->setColour (juce::TextButton::buttonOnColourId, colours::accent);
        addAndMakeVisible (*tab);
    }
    simpleTab.onClick = [this] { setView (true); };
    advancedTab.onClick = [this] { setView (false); };
    advancedTab.setComponentID ("advancedTab");

    autoButton.onClick = [this]
    {
        if (proc.getAutoState() == ForgeMasterProcessor::AutoState::listening)
            proc.cancelAutoMaster();
        else
        {
            proc.resetIntegrated();
            proc.startAutoMaster();
        }
    };
    addAndMakeVisible (autoButton);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setFont (juce::FontOptions (15.0f));
    statusLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (statusLabel);
    summaryLabel.setJustificationType (juce::Justification::centred);
    summaryLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (summaryLabel);

    // style parameter order: Balanced, Warm, Open, Flat
    const char* styleNames[3] { "Warm", "Balanced", "Open" };
    const int styleIndex[3] { 1, 0, 2 };
    const char* intensityNames[3] { "Low", "Medium", "High" };
    const float intensityValues[3] { 25.0f, 50.0f, 80.0f };
    for (int i = 0; i < 3; ++i)
    {
        for (auto* b : { &styleButtons[i], &intensityButtons[i] })
        {
            b->setColour (juce::TextButton::buttonOnColourId, colours::accent);
            addAndMakeVisible (*b);
        }
        styleButtons[i].setButtonText (styleNames[i]);
        styleButtons[i].onClick = [this, idx = styleIndex[i]]
        {
            auto* p = proc.apvts.getParameter ("style");
            p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        };
        loudnessButtons[i].setColour (juce::TextButton::buttonOnColourId, colours::accent);
        addAndMakeVisible (loudnessButtons[i]);
        loudnessButtons[i].setButtonText (intensityNames[i]);
        loudnessButtons[i].onClick = [this, idx = targets::simpleIndex[i]]
        {
            auto* p = proc.apvts.getParameter ("target");
            p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        };
        intensityButtons[i].setButtonText (intensityNames[i]);
        intensityButtons[i].onClick = [this, v = intensityValues[i]]
        {
            auto* p = proc.apvts.getParameter ("intensity");
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        };
    }

    const std::pair<const char*, const char*> macros[] { { "lowGain", "Low End" }, { "presGain", "Presence" },
                                                         { "airGain", "Air" }, { "width", "Stereo Width" },
                                                         { "satDrive", "Warmth" }, { "ceiling", "Ceiling" } };
    for (auto& [id, caption] : macros)
    {
        auto k = std::make_unique<Knob> (proc.apvts, id, caption);
        addAndMakeVisible (*k);
        simpleKnobs[id] = std::move (k);
    }

    setSize (1200, 740);
    setView (true);
    startTimerHz (30);
}

ForgeMasterEditor::~ForgeMasterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Knob& ForgeMasterEditor::knob (const juce::String& id, const juce::String& caption)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, caption);
    addAndMakeVisible (*k);
    return *(knobs[id] = std::move (k));
}

juce::ToggleButton& ForgeMasterEditor::toggle (const juce::String& id, const juce::String& caption)
{
    auto t = std::make_unique<juce::ToggleButton> (caption);
    addAndMakeVisible (*t);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *t));
    return *(toggles[id] = std::move (t));
}

juce::ComboBox& ForgeMasterEditor::combo (const juce::String& id, const juce::StringArray& items)
{
    auto c = std::make_unique<juce::ComboBox>();
    c->addItemList (items, 1);
    addAndMakeVisible (*c);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c));
    return *(combos[id] = std::move (c));
}

void ForgeMasterEditor::setView (bool simple)
{
    simpleView = simple;
    (simple ? simpleTab : advancedTab).setToggleState (true, juce::dontSendNotification);
    for (auto* c : advancedOnly) c->setVisible (! simple);
    for (auto& [id, k] : simpleKnobs) k->setVisible (simple);
    for (int i = 0; i < 3; ++i)
    {
        styleButtons[i].setVisible (simple);
        intensityButtons[i].setVisible (simple);
        loudnessButtons[i].setVisible (simple);
    }
    for (juce::Component* c : { (juce::Component*) &autoButton, (juce::Component*) &statusLabel, (juce::Component*) &summaryLabel })
        c->setVisible (simple);
    resized();
    repaint();
}

void ForgeMasterEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto header = r.removeFromTop (44);
    auto tabs = header.removeFromRight (220).withSizeKeepingCentre (220, 28);
    simpleTab.setBounds (tabs.removeFromLeft (110));
    advancedTab.setBounds (tabs);

    if (simpleView)
        layoutSimple (r);
    else
        layoutAdvanced (r);
}

void ForgeMasterEditor::layoutSimple (juce::Rectangle<int> r)
{
    simpleLeft = r.removeFromLeft (r.getWidth() / 2 - 6);
    r.removeFromLeft (12);
    simpleRight = r;

    auto a = simpleLeft.reduced (20);
    a.removeFromTop (16);
    autoButton.setBounds (a.removeFromTop (270).withSizeKeepingCentre (270, 270));
    statusLabel.setBounds (a.removeFromTop (28));
    summaryLabel.setBounds (a.removeFromTop (20));
    a.removeFromTop (18);

    auto buttonRow = [] (juce::Rectangle<int> row, juce::TextButton* buttons)
    {
        row = row.withSizeKeepingCentre (420, row.getHeight());
        const int w = row.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            buttons[i].setBounds (row.removeFromLeft (w).reduced (4, 0));
    };
    a.removeFromTop (18);   // "STYLE" caption
    buttonRow (a.removeFromTop (36), styleButtons);
    a.removeFromTop (30);   // "INTENSITY" caption
    buttonRow (a.removeFromTop (36), intensityButtons);
    a.removeFromTop (30);   // "LOUDNESS" caption
    buttonRow (a.removeFromTop (36), loudnessButtons);

    auto b = simpleRight.reduced (16);
    b.removeFromTop (24);
    const char* ids[6] { "lowGain", "presGain", "airGain", "width", "satDrive", "ceiling" };
    for (int row = 0; row < 2; ++row)
    {
        auto line = b.removeFromTop (118);
        const int w = line.getWidth() / 3;
        for (int k = 0; k < 3; ++k)
            simpleKnobs[ids[row * 3 + k]]->setBounds (line.removeFromLeft (w).withSizeKeepingCentre (100, 108));
    }
    b.removeFromTop (10);
    combos["target"]->setBounds (b.removeFromTop (28));
    b.removeFromTop (6);
    auto row = b.removeFromTop (26);
    const int third = row.getWidth() / 3;
    toggles["autoLoud"]->setBounds (row.removeFromLeft (third + 20));
    toggles["compare"]->setBounds (row.removeFromLeft (third - 20));
    toggles["gainMatch"]->setBounds (row);
    b.removeFromTop (8);
    meterArea = b.withTrimmedBottom (34);
    resetButton.setBounds (b.removeFromBottom (26).removeFromRight (100));
}

void ForgeMasterEditor::layoutAdvanced (juce::Rectangle<int> r)
{
    constexpr int kw = 88, kh = advancedKnobHeight;

    toneArea = r.removeFromLeft (290);
    r.removeFromLeft (12);
    outArea = r.removeFromRight (300);
    r.removeFromRight (12);
    bandsArea = r;

    // Tone column
    {
        auto a = toneArea.reduced (10);
        a.removeFromTop (22);
        auto row = a.removeFromTop (kh);
        knobs["inGain"]->setBounds (row.removeFromLeft (kw));
        knobs["intensity"]->setBounds (row.removeFromLeft (kw));
        combos["style"]->setBounds (row.reduced (4, 30));

        a.removeFromTop (28);
        const char* eqRows[4][2] { { "lowGain", "lowFreq" }, { "lmGain", "lmFreq" },
                                   { "presGain", "presFreq" }, { "airGain", "airFreq" } };
        for (auto& pair : eqRows)
        {
            row = a.removeFromTop (kh);
            row.removeFromLeft (30);
            knobs[pair[0]]->setBounds (row.removeFromLeft (kw + 20));
            knobs[pair[1]]->setBounds (row.removeFromLeft (kw + 20));
        }
    }

    // Multiband column
    {
        auto a = bandsArea.reduced (10);
        a.removeFromTop (22);
        auto row = a.removeFromTop (kh);
        knobs["xover1"]->setBounds (row.removeFromLeft (kw + 20));
        knobs["xover2"]->setBounds (row.removeFromLeft (kw + 20));
        knobs["xover3"]->setBounds (row.removeFromLeft (kw + 20));
        toggles["msMode"]->setBounds (row.removeFromLeft (120).withSizeKeepingCentre (110, 28));

        a.removeFromTop (14);
        const int colW = a.getWidth() / ForgeMasterProcessor::numBands;
        for (int b = 0; b < ForgeMasterProcessor::numBands; ++b)
        {
            auto col = a.removeFromLeft (colW).reduced (4, 0);
            const juce::String pre = "b" + juce::String (b + 1);
            col.removeFromTop (22);   // band title
            toggles[pre + "On"]->setBounds (col.removeFromTop (26).withSizeKeepingCentre (80, 24));
            const char* names[6] { "Thr", "Ratio", "Att", "Rel", "Gain", "Width" };
            for (int k = 0; k < 6; k += 2)
            {
                auto kr = col.removeFromTop (kh);
                knobs[pre + names[k]]->setBounds (kr.removeFromLeft (kr.getWidth() / 2));
                knobs[pre + names[k + 1]]->setBounds (kr);
            }
            col.removeFromTop (12);
            bandGrBars[b] = col.removeFromTop (14).reduced (6, 0);
        }
    }

    // Output column
    {
        auto a = outArea.reduced (10);
        a.removeFromTop (22);
        const char* grid[3][3] { { "width", "monoBass", "satDrive" },
                                 { "satMix", "drive", "ceiling" },
                                 { "limRelease", "customLufs", "outGain" } };
        for (auto& line : grid)
        {
            auto row = a.removeFromTop (kh);
            const int w = row.getWidth() / 3;
            for (auto* id : line)
                knobs[id]->setBounds (row.removeFromLeft (w));
        }
        a.removeFromTop (8);
        combos["target"]->setBounds (a.removeFromTop (26));
        a.removeFromTop (6);
        auto row = a.removeFromTop (26);
        const int third = row.getWidth() / 3;
        toggles["autoLoud"]->setBounds (row.removeFromLeft (third + 20));
        toggles["compare"]->setBounds (row.removeFromLeft (third - 20));
        toggles["gainMatch"]->setBounds (row);
        a.removeFromTop (8);
        meterArea = a.withTrimmedBottom (34);
        resetButton.setBounds (a.removeFromBottom (26).removeFromRight (100));
    }
}

void ForgeMasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("FORGE MASTER", 16, 12, 300, 30, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Multiband / Mid-Side mastering with loudness targeting", 200, 12, 500, 30,
                juce::Justification::centredLeft);

    auto drawPanel = [&] (juce::Rectangle<int> area, const juce::String& title)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (colours::accent);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, area.reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft);
    };
    if (simpleView)
    {
        drawPanel (simpleLeft, "ONE-CLICK MASTER");
        drawPanel (simpleRight, "FINE TUNE & LOUDNESS");
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        // Captions sit in their own strip just above each button row.
        auto caption = [] (const juce::TextButton& row) { return juce::Rectangle<int> (row.getX() + 4, row.getY() - 22, 320, 18); };
        g.drawText ("STYLE", caption (styleButtons[0]), juce::Justification::centredLeft);
        g.drawText ("INTENSITY", caption (intensityButtons[0]), juce::Justification::centredLeft);
        g.drawText ("LOUDNESS  (-14 / -11 / -9 LUFS)", caption (loudnessButtons[0]), juce::Justification::centredLeft);
        drawMeters (g);
        return;
    }

    drawPanel (toneArea, "INPUT & TONE");
    drawPanel (bandsArea, "MULTIBAND DYNAMICS");
    drawPanel (outArea, "STEREO, SATURATION & LOUDNESS");

    // EQ subtitle
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("TONE EQ", toneArea.getX() + 12, toneArea.getY() + 32 + advancedKnobHeight + 4, 200, 20, juce::Justification::centredLeft);

    // Band titles
    const char* bandNames[4] { "LOW", "LOW-MID", "HIGH-MID", "HIGH" };
    for (int b = 0; b < ForgeMasterProcessor::numBands; ++b)
    {
        auto bar = bandGrBars[b];
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (bandNames[b], bar.getX() - 6, bandsArea.getY() + 10 + 22 + advancedKnobHeight + 14, bar.getWidth() + 12, 20,
                    juce::Justification::centred);
    }

    drawMeters (g);
}

void ForgeMasterEditor::drawMeters (juce::Graphics& g)
{
    // Band gain reduction bars (0..-12 dB)
    for (int b = 0; b < (simpleView ? 0 : ForgeMasterProcessor::numBands); ++b)
    {
        auto bar = bandGrBars[b].toFloat();
        g.setColour (colours::bg);
        g.fillRoundedRectangle (bar, 3.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, -mBand[b] / 12.0f);
        g.setColour (colours::accent2);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("GR " + juce::String (mBand[b], 1) + " dB", bar.translated (0, 16).toNearestInt(),
                    juce::Justification::centred);
    }

    auto a = meterArea.toFloat();
    g.setColour (colours::bg);
    g.fillRoundedRectangle (a, 6.0f);
    a = a.reduced (12.0f, 8.0f);

    const float target = proc.getTargetLufs();
    auto fmt = [] (float v) { return v <= -99.0f ? juce::String ("--") : juce::String (v, 1); };

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("INTEGRATED", a.removeFromTop (16), juce::Justification::centredLeft);
    const bool onTarget = mI > -99.0f && std::abs (mI - target) <= 1.0f;
    g.setColour (onTarget ? colours::good : colours::text);
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    g.drawText (fmt (mI) + " LUFS", a.removeFromTop (40), juce::Justification::centredLeft);

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Target " + juce::String (target, 1) + " LUFS", a.removeFromTop (16), juce::Justification::centredLeft);
    a.removeFromTop (6);

    auto line = [&] (const juce::String& name, const juce::String& value, juce::Colour c)
    {
        auto r = a.removeFromTop (18);
        g.setColour (colours::dim);
        g.drawText (name, r, juce::Justification::centredLeft);
        g.setColour (c);
        g.drawText (value, r, juce::Justification::centredRight);
    };
    g.setFont (juce::FontOptions (13.0f));
    line ("Momentary", fmt (mM) + " LUFS", colours::text);
    line ("Short-term", fmt (mS) + " LUFS", colours::text);
    const float ceiling = proc.apvts.getRawParameterValue ("ceiling")->load();
    line ("True peak", fmt (mTp) + " dBTP", mTp > ceiling + 0.05f ? colours::warn : colours::text);
    line ("Limiter drive", (mDrive >= 0.0f ? "+" : "") + juce::String (mDrive, 1) + " dB", colours::accent);

    a.removeFromTop (6);
    g.setColour (colours::dim);
    g.drawText ("Limiter GR", a.removeFromTop (16), juce::Justification::centredLeft);
    auto bar = a.removeFromTop (10);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (colours::accent);
    const float frac = juce::jlimit (0.0f, 1.0f, -mGr / 12.0f);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
}

void ForgeMasterEditor::timerCallback()
{
    auto& m = proc.meters;
    mM = m.outMomentary.load();
    mS = m.outShortTerm.load();
    mI = m.outIntegrated.load();
    mTp = m.truePeakDb.load();
    mDrive = m.autoDriveDb.load();
    mGr = juce::jmin (m.limiterGrDb.load(), mGr * 0.9f);
    for (int b = 0; b < ForgeMasterProcessor::numBands; ++b)
        mBand[b] = juce::jmin (m.bandGrDb[b].load(), mBand[b] * 0.9f);
    using State = ForgeMasterProcessor::AutoState;
    const auto state = proc.getAutoState();
    const float progress = proc.getAutoProgress();
    if (state != autoButton.state || std::abs (progress - autoButton.progress) > 0.005f)
    {
        autoButton.state = state;
        autoButton.progress = progress;
        autoButton.repaint();
    }
    statusLabel.setText (state == State::listening ? "Play the track - ideally the fullest section. Click to cancel."
                         : state == State::mastered ? "Mastered. Pick a style and intensity, or fine tune."
                                                    : "Press play in your DAW, then click AUTO MASTER.",
                         juce::dontSendNotification);
    summaryLabel.setText (proc.getAutoSummary(), juce::dontSendNotification);

    const int style = (int) proc.apvts.getRawParameterValue ("style")->load();
    const float intensity = proc.apvts.getRawParameterValue ("intensity")->load();
    const int target = (int) proc.apvts.getRawParameterValue ("target")->load();
    const int styleIndex[3] { 1, 0, 2 };
    for (int i = 0; i < 3; ++i)
    {
        styleButtons[i].setToggleState (style == styleIndex[i], juce::dontSendNotification);
        const int nearest = intensity < 37.5f ? 0 : (intensity < 65.0f ? 1 : 2);
        intensityButtons[i].setToggleState (nearest == i, juce::dontSendNotification);
        loudnessButtons[i].setToggleState (target == targets::simpleIndex[i], juce::dontSendNotification);
    }

    repaint (meterArea);
    for (auto& bar : bandGrBars)
        repaint (bar.expanded (8, 0).withHeight (34));
}
