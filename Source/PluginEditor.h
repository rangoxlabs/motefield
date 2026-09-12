#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PerformancePanel.h"
#include "Appearance.h"
#include "TuningPanel.h"

class MoteFieldLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MoteFieldLookAndFeel();
    void setAppearance (bool dark, juce::Colour accent);
    appearance::Palette colours = appearance::palette (false, juce::Colour (appearance::acid));
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

class PerformanceSlider final : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent& event) override
    {
        setMouseDragSensitivity (event.mods.isShiftDown() ? 1440 : 180);
        juce::Slider::mouseDown (event);
    }
};

class ParameterKnob final : public juce::Component
{
public:
    ParameterKnob (const juce::String&, const juce::String&);
    void resized() override;
    void paint (juce::Graphics&) override;
    PerformanceSlider slider;
    juce::String title;
};

class FieldDisplay final : public juce::Component, public juce::TooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void update (const motefield::VisualFrame&);
    std::function<void(const char*, float, int)> gesture;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    juce::String getTooltip() override { return "Drag liquid to scan / transpose. Shift-drag stretches. Alt-drag splits voices. Double-click resets."; }
    void setEnvelopeVisible (bool show) { envelopeVisible = show; repaint(); }
    void invalidateMaterial() { materialRendered = false; }
    void setShape (float value) { shape = value; }
    void setSampleRate (double value) { sampleRate = value > 0.0 ? value : 48000.0; }
    void setReducedMotion (bool enabled) { reducedMotion = enabled; repaint(); }
    const motefield::VisualFrame& currentFrame() const { return frame; }
private:
    juce::Point<float> dragStart;
    float startX = 0.f, startY = 0.f; int gestureMode = 0; bool dragging = false;
    struct LiquidVoice
    {
        std::uint64_t id = 0;
        float x = .5f, y = .5f, radiusX = .04f, radiusY = .2f;
        float energy = 0.0f, heat = 0.0f, bend = 0.0f;
        bool seen = false;
    };
    struct SurfaceBody
    {
        float x = 240.f, y = 72.f, radius = 0.f, stretch = 1.f;
        float vx = 0.f, vy = 0.f, vr = 0.f, vs = 0.f;
    };
    std::array<SurfaceBody, 5> surfaceBodies {};
    float staleSeconds = 0.f, lowDrive = 0.f, midDrive = 0.f, highDrive = 0.f;
    float centreX = 240.f, centreY = 72.f, motionPhase = 0.f, transientDrive = 0.f, lastDrive = 0.f;
    void renderLiquid();
    static constexpr int liquidWidth = 480, liquidHeight = 240;
    std::array<LiquidVoice, 64> liquidVoices {};
    std::array<float, liquidWidth * liquidHeight> liquidField {}, liquidHeat {}, liquidGradientY {};
    juce::Image liquidImage { juce::Image::ARGB, liquidWidth, liquidHeight, true };
    double sampleRate = 48000.0;
    motefield::VisualFrame frame;
    bool envelopeVisible = true, reducedMotion = false, materialRendered = false, materialActive = false;
    float shape = .52f, outputDrive = 0.0f;
};

class LooperTape final : public juce::Component
{
public:
    LooperTape(MoteFieldAudioProcessor&,const FieldDisplay&);
    ~LooperTape() override { for(auto* p:gestures)p->endChangeGesture(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void update();
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
private:
    MoteFieldAudioProcessor& processor;
    const FieldDisplay& field;
    juce::Label startValue,endValue,fadeValue;
    juce::TextButton snapButton,fullButton;
    float seconds=0,first=0,last=1,fade=.02f;
    bool available=false;
    int dragging=0;
    float dragX=0,dragFirst=0,dragLast=1;
    std::vector<juce::RangedAudioParameter*> gestures;
    void setRange(float,float);
    void edit(juce::Label&,int);
    float snap(float) const;
    int hit(juce::Point<float>) const;
};

class MoteFieldAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    explicit MoteFieldAudioProcessorEditor (MoteFieldAudioProcessor&);
    ~MoteFieldAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshDisplay();
    void applyAppearance();
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void timerCallback() override { refreshDisplay(); }
    void addKnob (const juce::String&, const char*, const juce::String&);
    void setupButton (juce::TextButton&, const juce::String&, const juce::String&, bool = false, int = 0);
    void setupCombo (juce::ComboBox&, const juce::StringArray&, const juce::String&);
    void updateTimeAttachment();
    void tapTempo();
    void setDetailsOpen (bool);
    void scrollBarMoved (juce::ScrollBar*, double) override;
    juce::ScrollBar detailsScroll { true };
    float scrollOffset = 0.f;
    void loadPreset (int);
    void refreshPresetMenu();
    void selectPresetItem (int);
    void stepPreset (int);
    void showSavePresetDialog();
    void savePresetNamed (const juce::String&, bool overwrite = false);
    float value (const char*) const;
    juce::String currentHelp() const;

    MoteFieldAudioProcessor& processor;
    juce::SharedResourcePointer<appearance::Preferences> appearancePreferences;
    int appearanceRevision = -1;
    MoteFieldLookAndFeel lookAndFeel;
    juce::TextButton settingsButton;
    std::unique_ptr<appearance::SettingsPanel> settingsPanel;
    juce::Image hardwarePanel, rangoLogo;
    int cachedGlassFinish = -1;
    juce::String cachedPrintPreview;
    float logoDrive = 0.0f;
    PerformanceSlider modeSelector;
    std::unique_ptr<SliderAttachment> modeAttachment;
    FieldDisplay fieldDisplay;
    LooperTape looperTape;
    juce::TooltipWindow tooltips;
    std::array<juce::TextButton, 11> modeButtons;
    std::array<juce::TextButton, 4> variationButtons;
    std::vector<std::unique_ptr<ParameterKnob>> knobs;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<SliderAttachment> timeAttachment;
    juce::TextButton reverseButton, syncButton, tapButton, holdButton, bypassButton, wetSoloButton, levelMatchButton, reverbSoloButton;
    juce::Label matchStatus;
    std::unique_ptr<TuningPanel> tuningPanel;
    juce::TextButton recordButton, playButton, dubButton, stopButton, undoButton, eraseButton;
    juce::TextButton preButton, postButton, loopReverseButton, detailsButton, motionButton;
    juce::TextButton previousPreset, nextPreset, savePresetButton, randomPresetButton, performanceButton;
    std::unique_ptr<PerformancePanel> performancePanel;
    bool momentaryHoldDown = false;
    juce::ComboBox presetBox, roomBox, speedBox, divisionBox;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    juce::Label tempoLabel, timeValueLabel;
    bool detailsOpen = false, lastSync = false;
    double lastTap = 0.0, tapInterval = 0.0;
    int tapCount = 0, tapFlash = 0;
    juce::Array<juce::File> userPresets;
    std::unique_ptr<juce::AlertWindow> presetNameDialog;
    juce::String helpText;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoteFieldAudioProcessorEditor)
};
