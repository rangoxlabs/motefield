#pragma once
#include "PluginProcessor.h"
#include "Appearance.h"

class WaveDragButton final : public juce::TextButton
{
public:
    std::function<juce::File()> makeFile;
    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! dragged && event.getDistanceFromDragStart() > 7 && makeFile)
        { dragged = true; const auto file = makeFile(); if (file.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this); }
    }
    void mouseDown (const juce::MouseEvent& event) override { dragged = false; juce::TextButton::mouseDown (event); }
    void mouseUp (const juce::MouseEvent& event) override { if (! dragged) juce::TextButton::mouseUp (event); else setState (juce::Button::buttonNormal); }
private:
    bool dragged = false;
};

class PerformancePanel final : public juce::Component, private juce::Timer
{
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox choice;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        int page = 0;
    };
public:
    explicit PerformancePanel (MoteFieldAudioProcessor& owner) : processor (owner)
    {
        setOpaque (true); setComponentID ("performance-panel");
        addAndMakeVisible (close); close.setComponentID ("perform-close"); close.setButtonText ("BACK TO INSTRUMENT"); close.onClick = [this] { setVisible (false); };
        addAndMakeVisible (pages); pages.setComponentID ("perform-pages"); pages.addItemList ({ "LOOP & CAPTURE", "MATERIAL & MAGNET", "PATTERN & MIDI" }, 1); pages.setSelectedId (1);
        pages.onChange = [this] { resized(); };
        addAndMakeVisible (viewport); viewport.setViewedComponent (&content, false); viewport.setScrollBarsShown (true, false);
        for (const auto& id : MoteFieldAudioProcessor::extendedParameterIds())
        {
            if (id == "tuningReference") continue; // Edited in the dedicated tuning page.
            auto row = std::make_unique<Row>(); auto* parameter = processor.parameters.getParameter (id);
            row->page = id.startsWith ("loop") || id == "burstGate" || id == "bypassTrails" || id == "holdStyle" ? 1
                      : id.startsWith ("pattern") || id.contains ("Mutation") || id.startsWith ("scale") || id == "sourceNote" ? 3 : 2;
            row->label.setText (parameter->getName (64), juce::dontSendNotification); row->label.setColour (juce::Label::textColourId, juce::Colour (0xff24281e));
            content.addAndMakeVisible (row->label);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
            { row->choice.addItemList (choice->choices, 1); content.addAndMakeVisible (row->choice); row->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.parameters, id, row->choice); row->choice.setComponentID (id); }
            else
            { row->slider.setSliderStyle (juce::Slider::LinearHorizontal); row->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 24);
              row->slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff24281e)); row->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xffeeead7));
              row->slider.setNumDecimalPlacesToDisplay (parameter->getNormalisableRange().interval >= 1 ? 0 : 2);
              content.addAndMakeVisible (row->slider); row->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, id, row->slider); row->slider.setComponentID (id);
              const bool percent = id == "viscosity" || id == "cohesion" || id == "tension" || id == "fieldPosition" || id == "fieldSplit" || id == "magnetAmount";
              const bool ratio = id == "loopRate" || id == "fieldStretch", pitch = id == "fieldPitch", milliseconds = id == "magnetAttack" || id == "magnetRelease";
              const bool integer = parameter->getNormalisableRange().interval >= 1;
              row->slider.textFromValueFunction = [percent,ratio,pitch,milliseconds,integer] (double v)
              { if (percent) return juce::String (juce::roundToInt (v * 100)) + "%";
                if (milliseconds) return juce::String (juce::roundToInt (v * 1000)) + " ms";
                if (integer) return juce::String (juce::roundToInt (v));
                if (std::abs (v) < .005) v = 0;
                return juce::String (v, 2) + (ratio ? "x" : pitch ? " st" : " s"); };
              row->slider.valueFromTextFunction = [percent,milliseconds] (const juce::String& text)
              { return text.getDoubleValue() / (percent ? 100.0 : milliseconds ? 1000.0 : 1.0); };
              row->slider.updateText();
              row->slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff899471)); }
            rows.push_back (std::move (row));
        }
        content.addAndMakeVisible (instructions); instructions.setColour (juce::Label::textColourId, juce::Colour (0xff4a4f3f));
        instructions.setJustificationType (juce::Justification::topLeft);
        for (int i = 0; i < 3; ++i)
        { auto& button = captures[static_cast<std::size_t> (i)]; content.addAndMakeVisible (button); const auto bars = 1 << i;
          button.setButtonText ("CAPTURE " + juce::String (bars) + (bars == 1 ? " BAR" : " BARS"));
          button.setTooltip ("Replace the phrase with the most recent output audio.");
          button.onClick = [this,bars] { report (processor.captureHistory (bars), "Captured recent output into the phrase looper."); }; }
        content.addAndMakeVisible (burst); burst.setButtonText ("HOLD TO CAPTURE BURST");
        burst.onStateChange = [this] { const auto down = burst.isDown(); if (down != burstDown) { burstDown = down; processor.setParameterValue ("burstGate", down ? 1.f : 0.f); } };
        content.addAndMakeVisible (exportButton); exportButton.setButtonText ("EXPORT / DRAG LOOP WAV");
        exportButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Export recorded phrase", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("MoteField.wav"), "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [safe = juce::Component::SafePointer<PerformancePanel> (this)] (const juce::FileChooser& fc)
                { if (safe && fc.getResult() != juce::File {}) safe->report (safe->processor.exportAudio (fc.getResult().withFileExtension ("wav")), "WAV exported."); });
        };
        exportButton.makeFile = [this]
        {
            const auto folder = MoteFieldAudioProcessor::userPresetDirectory().getSiblingFile ("Exports"); folder.createDirectory();
            const auto file = folder.getNonexistentChildFile ("MoteField phrase", ".wav");
            const auto result = processor.exportAudio (file); report (result, "Dragged WAV is kept in the MoteField Exports folder."); return result.wasOk() ? file : juce::File {};
        };
        content.addAndMakeVisible (learnTarget);
        int index = 1; for (const auto* parameter : processor.getParameters()) learnTarget.addItem (parameter->getName (64), index++);
        learnTarget.setSelectedId (1);
        content.addAndMakeVisible (learn); learn.setButtonText ("LEARN NEXT MIDI CC"); learn.onClick = [this] { processor.learnMidi (learnTarget.getSelectedId() - 1); status.setText ("Move the MIDI control you want to assign.", juce::dontSendNotification); };
        content.addAndMakeVisible (clearMidi); clearMidi.setButtonText ("CLEAR MIDI MAPPINGS"); clearMidi.onClick = [this] { processor.clearMidiMappings(); status.setText ("MIDI mappings cleared.", juce::dontSendNotification); };
        content.addAndMakeVisible (mutateRhythm); content.addAndMakeVisible (mutatePitch);
        mutateRhythm.setButtonText ("MUTATE RHYTHM"); mutatePitch.setButtonText ("MUTATE PITCH");
        mutateRhythm.onClick = [this] { mutate ("rhythmMutation"); }; mutatePitch.onClick = [this] { mutate ("pitchMutation"); };
        addAndMakeVisible (status); status.setColour (juce::Label::textColourId, juce::Colour (0xff24281e));
        for (auto* button : std::array<juce::TextButton*, 10> { &close, &burst, &exportButton, &learn, &clearMidi, &mutateRhythm, &mutatePitch, &captures[0], &captures[1], &captures[2] })
        { button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xffded6ba)); button->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcbd69d)); button->getProperties().set ("panelButton",true); }
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,juce::Colour (0xff899471));
        startTimerHz (10);
    }
    ~PerformancePanel() override { stopTimer(); if (burstDown) processor.setParameterValue ("burstGate",0.f); }
    void lookAndFeelChanged() override
    {
        const auto p = appearance::read (*this);
        for (auto& row : rows)
        {
            row->label.setColour (juce::Label::textColourId,p.ink);
            row->slider.setColour (juce::Slider::textBoxTextColourId,p.ink);
            row->slider.setColour (juce::Slider::textBoxBackgroundColourId,p.paper);
            row->slider.setColour (juce::Slider::thumbColourId,p.cyan);
        }
        instructions.setColour (juce::Label::textColourId,p.muted);
        status.setColour (juce::Label::textColourId,p.ink);
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,p.muted);
    }
    void paint (juce::Graphics& g) override
    { const auto p=appearance::read (*this); g.fillAll (p.paper); g.setColour (p.line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2),14,2); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (18); auto top = area.removeFromTop (36);
        close.setBounds (top.removeFromRight (190)); top.removeFromRight (12); pages.setBounds (top);
        area.removeFromTop (10); status.setBounds (area.removeFromBottom (40)); viewport.setBounds (area);
        const int width = juce::jmax (200, viewport.getWidth() - 16), page = pages.getSelectedId(); int y = 92;
        const int columns = width >= 700 ? 2 : 1, cell = width / columns;
        int index = 0;
        for (auto& row : rows)
        {
            const bool visible = row->page == page; row->label.setVisible (visible); row->slider.setVisible (visible && row->sliderAttachment != nullptr); row->choice.setVisible (visible && row->comboAttachment != nullptr);
            if (! visible) continue;
            const auto x = (index % columns) * cell;
            row->label.setBounds (x + 8, y + (index / columns) * 70, cell - 24, 24);
            row->slider.setBounds (x + 8, y + (index / columns) * 70 + 26, cell - 24, 32); row->choice.setBounds (row->slider.getBounds()); ++index;
        }
        y += ((index + columns - 1) / columns) * 70 + 12;
        for (auto& b : captures) b.setVisible (page == 1);
        burst.setVisible (page == 1); exportButton.setVisible (page == 1);
        learnTarget.setVisible (page == 3); learn.setVisible (page == 3); clearMidi.setVisible (page == 3); mutateRhythm.setVisible (page == 3); mutatePitch.setVisible (page == 3);
        if (page == 1)
        { for (int i = 0; i < 3; ++i) captures[static_cast<std::size_t> (i)].setBounds (8 + i * (width / 3), y, width / 3 - 16, 36);
          burst.setBounds (8, y + 46, width / 2 - 16, 40); exportButton.setBounds (width / 2 + 8, y + 46, width / 2 - 16, 40); y += 100; }
        if (page == 3)
        { mutateRhythm.setBounds (8,y,width / 2 - 16,36); mutatePitch.setBounds (width / 2 + 8,y,width / 2 - 16,36);
          learnTarget.setBounds (8,y + 46,width - 16,34); learn.setBounds (8,y + 90,width / 2 - 16,36); clearMidi.setBounds (width / 2 + 8,y + 90,width / 2 - 16,36); y += 140; }
        instructions.setBounds (8, 6, width - 16, 80);
        instructions.setText (page == 1 ? "Record immediately, on the next beat/bar, or with a one-bar count-in. Fixed lengths finish automatically (60-second maximum). Synced arming waits for host playback; SUBDIV off uses internal 4/4. Follow loop quantize keeps legacy Beat/Off behavior. Loops save with projects and user presets. Capture replaces the phrase; drag WAV to export."
            : page == 2 ? "On the fluid: drag to scan / transpose, Shift-drag to stretch, Alt-drag to split. Double-click resets the gesture. Viscosity slows the response; Cohesion gathers voices; Tension sharpens their envelope. Feed the Magnet sidechain from another track."
            : "Lock repeats the chosen event pattern; mutations change rhythm or pitch separately. Set Source note to your input's tonal center, then choose a root and scale. This constrains transposition, not individual notes inside a chord. Program changes 1-32 select factory presets.", juce::dontSendNotification);
        content.setSize (width, y); viewport.setViewPosition (0,0);
    }
private:
    void report (const juce::Result& result, const juce::String& success) { status.setText (result.wasOk() ? success : result.getErrorMessage(), juce::dontSendNotification); }
    void mutate (const char* id) { const auto value = static_cast<int> (processor.parameters.getRawParameterValue (id)->load()); processor.setParameterValue (id, static_cast<float> ((value + 1) % 128)); }
    void timerCallback() override { const auto cc = processor.learnedController(); if (cc != lastCC) { lastCC = cc; if (cc >= 0) status.setText ("Assigned MIDI CC " + juce::String (cc) + ". Mapping is saved with this project.", juce::dontSendNotification); } }
    MoteFieldAudioProcessor& processor;
    juce::TextButton close, burst, learn, clearMidi, mutateRhythm, mutatePitch;
    WaveDragButton exportButton;
    juce::ComboBox pages, learnTarget;
    juce::Viewport viewport; juce::Component content;
    juce::Label instructions, status;
    std::array<juce::TextButton, 3> captures;
    std::vector<std::unique_ptr<Row>> rows;
    std::unique_ptr<juce::FileChooser> chooser;
    bool burstDown = false; int lastCC = -1;
};
