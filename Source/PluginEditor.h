#pragma once

#include "PluginProcessor.h"

class ForgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ForgeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Rotary knob with a caption, bound to one parameter.
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& caption);
    void resized() override;

    juce::Slider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// The big one-click button: idle -> listening (progress ring) -> mastered.
struct AutoMasterButton : public juce::Component
{
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    std::function<void()> onClick;
    ForgeMasterProcessor::AutoState state = ForgeMasterProcessor::AutoState::idle;
    float progress = 0.0f;
};

class ForgeMasterEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit ForgeMasterEditor (ForgeMasterProcessor&);
    ~ForgeMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    Knob& knob (const juce::String& id, const juce::String& caption);
    juce::ToggleButton& toggle (const juce::String& id, const juce::String& caption);
    juce::ComboBox& combo (const juce::String& id, const juce::StringArray& items);
    void drawMeters (juce::Graphics&);
    void setView (bool simple);
    void layoutSimple (juce::Rectangle<int> area);
    void layoutAdvanced (juce::Rectangle<int> area);

    ForgeMasterProcessor& proc;
    ForgeLookAndFeel lnf;

    std::map<juce::String, std::unique_ptr<Knob>> knobs;
    std::map<juce::String, std::unique_ptr<juce::ToggleButton>> toggles;
    std::map<juce::String, std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    juce::TextButton resetButton { "Reset LUFS" };

    // Simple (one-click) view
    bool simpleView = true;
    juce::TextButton simpleTab { "Simple" }, advancedTab { "Advanced" };
    AutoMasterButton autoButton;
    juce::Label statusLabel, summaryLabel;
    juce::TextButton styleButtons[3], intensityButtons[3], loudnessButtons[3];
    std::map<juce::String, std::unique_ptr<Knob>> simpleKnobs;
    std::vector<juce::Component*> advancedOnly;
    juce::Rectangle<int> simpleLeft, simpleRight;

    juce::Rectangle<int> toneArea, bandsArea, outArea, meterArea;
    juce::Rectangle<int> bandGrBars[ForgeMasterProcessor::numBands];

    float mM = -100, mS = -100, mI = -100, mTp = -100, mGr = 0, mDrive = 0;
    float mBand[ForgeMasterProcessor::numBands] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeMasterEditor)
};
