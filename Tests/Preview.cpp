#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <set>

namespace
{
void require (bool condition, const char* message) { if (! condition) throw std::runtime_error (message); }

juce::Component* find (juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id) return &parent;
    for (auto* child : parent.getChildren()) if (auto* result = find (*child, id)) return result;
    return nullptr;
}
void pump() { juce::Timer::callPendingTimersSynchronously(); }
void click (MoteFieldAudioProcessorEditor& editor, const juce::String& id)
{
    auto* button = dynamic_cast<juce::Button*> (find (editor, id));
    require (button != nullptr, "button not found");
    if (button->getClickingTogglesState()) button->setToggleState (! button->getToggleState(), juce::sendNotificationSync);
    else if (button->onClick) button->onClick();
    pump();
    editor.refreshDisplay();
}
struct PlayHead final : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (96.0);
        info.setIsPlaying (true);
        return info;
    }
};
float pluck (int sample)
{
    const auto t = static_cast<double> (sample) / 48000.0;
    const auto phase = std::fmod (t, .3);
    const std::array<double, 8> notes { 110.0, 164.8138, 220.0, 277.1826, 329.6276, 220.0, 164.8138, 130.8128 };
    const auto frequency = notes[static_cast<std::size_t> (static_cast<int> (t / .3) % 8)];
    const auto angle = juce::MathConstants<double>::twoPi * frequency * phase;
    const auto attack = std::min (1.0, phase * 600.0);
    return static_cast<float> (.31 * attack * std::exp (-phase * 13.0)
                              * (std::sin (angle) + .28 * std::sin (angle * 2.002) + .09 * std::sin (angle * 3.01)));
}
void saveImage (MoteFieldAudioProcessorEditor& editor, const juce::File& destination)
{
    auto stream = destination.createOutputStream();
    require (stream != nullptr, "cannot create screenshot");
    require (stream->setPosition (0) && stream->truncate().wasOk(), "cannot reset screenshot file");
    juce::PNGImageFormat format;
    require (format.writeImageToStream (editor.createComponentSnapshot (editor.getLocalBounds()), *stream), "screenshot failed");
}

struct HostParameterObserver final : juce::AudioProcessorParameter::Listener
{
    int values = 0, starts = 0, ends = 0;
    void parameterValueChanged (int, float) override { ++values; }
    void parameterGestureChanged (int, bool starting) override { starting ? ++starts : ++ends; }
};

void checkMaterialResponse (const juce::File& destination)
{
    FieldDisplay display;display.setSize (718, 198);display.setSampleRate (48000.0);
    motefield::VisualFrame frame;display.update (frame);
    const auto snapshot = [&] { return display.createComponentSnapshot (display.getLocalBounds()); };
    const auto rest = snapshot();
    const auto changed = [] (const juce::Image& a, const juce::Image& b)
    {
        int result = 0;
        for (int y = 47; y < 177; ++y) for (int x = 28; x < 480; ++x)
            if (a.getPixelAt (x, y) != b.getPixelAt (x, y)) ++result;
        return result;
    };
    std::array<juce::Image, 3> bands;
    for (std::size_t band = 0; band < bands.size(); ++band)
    {
        frame.spectralEnergy = {};frame.spectralEnergy[band] = .16f;frame.outputLevel = .2f;
        for (int i = 0; i < 120; ++i) { frame.sampleTime += 800;display.update (frame); }
        bands[band] = snapshot();
        require (changed (rest, bands[band]) > 400, "main material did not react to audio band");
        const auto name = juce::StringArray { "bass", "mid", "treble" }[static_cast<int> (band)];
        auto stream = destination.getChildFile ("material-" + name + ".png").createOutputStream();
        require (stream != nullptr && stream->setPosition (0) && stream->truncate().wasOk(), "material screenshot could not be written");
        require (juce::PNGImageFormat().writeImageToStream (bands[band], *stream), "material screenshot failed");
    }
    require (changed (bands[0], bands[1]) > 400 && changed (bands[1], bands[2]) > 400, "audio bands produce the same material response");
    for (int i = 0; i < 3; ++i) display.update (frame);
    require (changed (bands[2], snapshot()) < 100, "material flickered between audio snapshots");
    frame.outputLevel = 0.f;frame.spectralEnergy = {};
    for (int i = 0; i < 300; ++i) { frame.sampleTime += 800;display.update (frame); }
    require (changed (rest, snapshot()) == 0, "material did not return to its resting surface");
    std::cout << "Material checks passed: distinct bass/mid/treble deformation, snapshot-gap stability and silence settling.\n";
}

void checkPresetsAndAutomation (MoteFieldAudioProcessor& processor, const juce::File& destination)
{
    using namespace motefield::parameter;
    const auto directory = destination.getNonexistentChildFile ("preset-checks", {}, false);
    const auto names = MoteFieldAudioProcessor::factoryPresetNames();
    require (names.size() == 32, "factory preset count differs from the bank");
    std::set<int> modes;
    for (int i = 0; i < names.size(); ++i)
    {
        processor.applyFactoryPreset (i);
        require (processor.currentPresetName() == names[i] && ! processor.isPresetModified(), "factory preset metadata failed");
        modes.insert (static_cast<int> (processor.parameters.getRawParameterValue (mode)->load()));
    }
    require (modes.size() == 11, "factory bank does not cover every mode");
    processor.applyFactoryPreset (18);
    processor.setParameterValue (shape, .8123f);
    processor.setParameterValue (tempo, 137.4f);
    processor.setParameterValue (looperSpeed, 2.f);
    processor.setParameterValue (looperOrder, 1.f);
    processor.setParameterValue (modDepth, .000001f);
    require (processor.saveUserPreset ("../escape", false, directory).failed(), "preset filename escaped its folder");
    require (processor.saveUserPreset ("CON", false, directory).failed(), "Windows reserved preset name accepted");
    require (processor.saveUserPreset ("Orbital test", false, directory).wasOk(), "user preset save failed");
    const auto file = directory.getChildFile ("Orbital test.motefield");
    const auto original = file.loadFileAsString();
    require (! processor.isPresetModified(), "freshly saved preset is marked modified");
    processor.setParameterValue (shape, .1f);
    require (processor.isPresetModified(), "modified preset marker failed");
    require (processor.saveUserPreset ("Orbital test", false, directory).failed() && file.loadFileAsString() == original, "existing preset overwritten without permission");
    processor.applyFactoryPreset (0);
    processor.setParameterValue (freeze, 1.f);
    processor.setParameterValue (bypass, 1.f);
    require (processor.loadUserPreset (file).wasOk(), "user preset load failed");
    require (std::abs (processor.parameters.getRawParameterValue (shape)->load() - .8123f) < .0001f, "user shape did not round trip");
    require (std::abs (processor.parameters.getRawParameterValue (tempo)->load() - 137.4f) < .11f, "user tempo did not round trip");
    require (processor.parameters.getRawParameterValue (looperSpeed)->load() == 2.f && processor.parameters.getRawParameterValue (looperOrder)->load() == 1.f, "loop routing did not round trip");
    require (processor.parameters.getRawParameterValue (freeze)->load() == 1.f && processor.parameters.getRawParameterValue (bypass)->load() == 1.f, "preset changed Hold or Bypass");
    require (MoteFieldAudioProcessor::userPresetFiles (directory).size() == 1, "saved preset discovery failed");
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    {
        MoteFieldAudioProcessor reopened;
        reopened.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
        require (reopened.currentPresetName() == "Orbital test" && reopened.currentPresetSource() == "user" && ! reopened.isPresetModified(), "preset identity did not survive session restore");
    }
    const auto damaged = directory.getChildFile ("broken.motefield");
    require (damaged.replaceWithText ("<MoteFieldPreset version=\"1\"><PARAM id=\"shape\" value=\"0.2\"/></MoteFieldPreset>"), "could not create invalid preset fixture");
    require (processor.loadUserPreset (damaged).failed(), "incomplete preset was accepted");
    require (std::abs (processor.parameters.getRawParameterValue (shape)->load() - .8123f) < .0001f, "invalid preset partially changed sound");
    require (processor.saveUserPreset ("Orbital test", true, directory).wasOk(), "explicit replacement failed");

    require (processor.getParameters().size() == 58, "host parameter count changed unexpectedly");
    std::set<juce::String> ids;
    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        require (ranged != nullptr && ranged->isAutomatable(), "parameter not host automatable");
        require (ids.insert (ranged->paramID).second, "duplicate host parameter ID");
        HostParameterObserver host;
        parameter->addListener (&host);
        const auto target = ranged->convertFrom0to1 (parameter->getValue() > .5f ? .25f : .75f);
        processor.setParameterValue (ranged->paramID.toRawUTF8(), target);
        require (host.values > 0 && host.starts == 1 && host.ends == 1, "UI change did not notify host with a complete gesture");
        parameter->removeListener (&host);
        parameter->setValueNotifyingHost (ranged->convertTo0to1 (ranged->convertFrom0to1 (.5f)));
        require (std::abs (processor.parameters.getRawParameterValue (ranged->paramID)->load() - ranged->convertFrom0to1 (parameter->getValue())) < .001f, "host value did not reach processor");
    }
    // State recall discards transient commands instead of recording or erasing a loop.
    processor.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    juce::AudioBuffer<float> block (2, 400);juce::MidiBuffer midi;
    const auto tick = [&] { block.clear();processor.processBlock (block, midi); };
    tick();
    require (processor.getLooperState() == motefield::LooperState::empty, "session restore fired transport automation");
    const auto trigger = [&] (int index)
    {
        auto* parameter = processor.parameters.getParameter (looperTriggers[static_cast<std::size_t> (index)]);
        parameter->setValueNotifyingHost (parameter->convertFrom0to1 (parameter->getValue()) > .5f ? 0.f : 1.f);
        tick();
    };
    trigger (0); require (processor.getLooperState() == motefield::LooperState::recording, "host Record trigger failed");
    auto* recordTrigger = processor.parameters.getParameter (looperTriggers[0]);
    recordTrigger->setValueNotifyingHost (recordTrigger->getValue()); tick();
    require (processor.getLooperState() == motefield::LooperState::recording, "holding a trigger repeated its command");
    trigger (1); require (processor.getLooperState() == motefield::LooperState::playing, "host Play trigger failed");
    trigger (2); require (processor.getLooperState() == motefield::LooperState::overdubbing, "host Dub trigger failed");
    trigger (2); require (processor.getLooperState() == motefield::LooperState::playing, "repeated Dub automation edge failed");
    trigger (4);
    trigger (3); require (processor.getLooperState() == motefield::LooperState::stopped, "host Stop trigger failed");
    trigger (5); require (processor.getLooperState() == motefield::LooperState::empty, "host Erase trigger failed");
    processor.setParameterValue (freeze, 0.f);processor.setParameterValue (bypass, 0.f);
    std::cout << "Preset/automation checks passed: 32 factory presets, all 11 modes, disk round trip, overwrite protection, invalid-file rejection, session identity, 58 host parameters and looper triggers.\n";
}
void checkPerformanceIntegration (const juce::File& root)
{
    const auto destination = root.getNonexistentChildFile ("integration-checks", {}, false);
    require (destination.createDirectory().wasOk(),"integration folder creation failed");
    MoteFieldAudioProcessor p; p.prepareToPlay (48000,400);
    p.setParameterValue ("mix",0); p.setParameterValue ("looperOrder",1); p.setParameterValue ("looperLevel",1);
    juce::AudioBuffer<float> block (2,400); juce::MidiBuffer midi;
    const auto tick = [&] { for (int c = 0; c < 2; ++c) for (int i = 0; i < 400; ++i) block.setSample (c,i,.15f); p.processBlock (block,midi); };
    int recordIndex = -1; for (auto* param : p.getParameters()) if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param)) if (ranged->paramID == "loopRecordTrigger") recordIndex = param->getParameterIndex();
    require (recordIndex >= 0,"record parameter absent"); p.learnMidi (recordIndex);
    midi.addEvent (juce::MidiMessage::controllerEvent (1,20,127),128); tick();
    require (p.getLooperState() == motefield::LooperState::recording,"MIDI learn did not start recording");
    midi.addEvent (juce::MidiMessage::controllerEvent (1,20,0),0); tick();
    require (p.getLooperState() == motefield::LooperState::recording,"MIDI release retriggered record");
    p.requestLooperCommand (motefield::LooperCommand::play); tick();
    auto data = p.loopData(); juce::MemoryInputStream dataReader (data,false); dataReader.setPosition (13);
    require (dataReader.readInt() == 672,"MIDI record command did not respect sample offset");
    const auto wav = destination.getChildFile ("loop-export.wav"); require (p.exportAudio (wav).wasOk(),"WAV export failed");
    juce::AudioFormatManager formats; formats.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (wav));
    require (reader != nullptr && reader->lengthInSamples == 672 && reader->numChannels == 2,"WAV dimensions incorrect");
    juce::MemoryBlock saved; p.getStateInformation (saved);
    MoteFieldAudioProcessor reopened; reopened.setStateInformation (saved.getData(),static_cast<int> (saved.getSize())); reopened.prepareToPlay (48000,400);
    block.clear(); reopened.processBlock (block,midi);
    require (reopened.getLooperState() == motefield::LooperState::playing && reopened.loopData() == data,"session loop audio did not round trip");
    auto layouts = reopened.getBusesLayout(); layouts.inputBuses.set (1,juce::AudioChannelSet::mono()); require (reopened.setBusesLayout (layouts),"mono magnet bus layout rejected");
    juce::AudioBuffer<float> sideBlock (3,400); sideBlock.clear(); for (int i = 0; i < 400; ++i) sideBlock.setSample (2,i,.8f);
    reopened.setParameterValue ("magnetAmount",1); for (int tick = 0; tick < 4; ++tick) reopened.processBlock (sideBlock,midi);
    motefield::VisualFrame frame; reopened.readVisualFrame (frame); require (frame.magnet > 0,"sidechain did not reach engine");
    require (p.saveUserPreset ("Loop archive",false,destination).wasOk(),"audio preset save failed");
    p.requestLooperCommand (motefield::LooperCommand::clear); tick();
    require (p.loadUserPreset (destination.getChildFile ("Loop archive.motefield")).wasOk(),"audio preset load failed"); tick();
    require (p.loopData() == data,"preset loop audio did not round trip");
    require (p.captureHistory (1).wasOk(),"history capture failed"); tick();
    require (p.loopData().getSize() > data.getSize(),"history capture did not replace loop");
    juce::MemoryBlock broken (data); static_cast<char*> (broken.getData())[0] = 0;
    require (! p.restoreLoopData (broken),"corrupt audio archive accepted");
    std::cout << "Integration: sample-offset MIDI learn, release behavior, WAV export, session/preset audio recall, sidechain routing, history capture and corrupt-audio rejection passed.\n";
}
}

int main (int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const juce::File destination (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("Design").getFullPathName());
        require (destination.createDirectory().wasOk(), "cannot create preview directory");
        MoteFieldAudioProcessor processor;
        checkPerformanceIntegration (destination);
        processor.prepareToPlay (48000.0, 400);
        PlayHead playhead;
        processor.setPlayHead (&playhead);
        std::unique_ptr<MoteFieldAudioProcessorEditor> editor (static_cast<MoteFieldAudioProcessorEditor*> (processor.createEditor()));
        editor->setVisible (true);
        require (editor->getWidth() <= 1000 && editor->getHeight() <= 645, "initial editor is too large");
        for (const auto* id : { motefield::parameter::density, motefield::parameter::shape, motefield::parameter::mix })
        {
            auto* knob = dynamic_cast<juce::Slider*> (find (*editor, id));
            require (knob != nullptr && knob->getTextFromValue (.984136) == "98%", "percentage formatting is not rounded");
        }
        auto* field = dynamic_cast<FieldDisplay*> (find (*editor,"field")); require (field != nullptr,"interactive field missing");
        auto* pitchParameter = processor.parameters.getParameter ("fieldPitch"); HostParameterObserver fieldHost; pitchParameter->addListener (&fieldHost);
        const auto makeMouse = [&] (float x,float y,int clicks)
        { return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),{x,y},juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),1.f,0.f,0.f,0.f,0.f,field,field,juce::Time::getCurrentTime(),{100.f,80.f},juce::Time::getCurrentTime(),clicks,true); };
        field->mouseDown (makeMouse (100,80,1)); field->mouseDrag (makeMouse (140,60,1)); field->mouseUp (makeMouse (140,60,1));
        require (processor.parameters.getRawParameterValue ("fieldPitch")->load() > 0 && fieldHost.starts == 1 && fieldHost.ends == 1,"fluid drag did not notify host with a complete gesture");
        field->mouseDown (makeMouse (100,80,2)); field->mouseDoubleClick (makeMouse (100,80,2)); field->mouseUp (makeMouse (100,80,2));
        require (std::abs (processor.parameters.getRawParameterValue ("fieldPitch")->load()) < .001f,"double-click did not reset field pitch");
        require (fieldHost.starts == fieldHost.ends,"double-click left an automation gesture open"); pitchParameter->removeListener (&fieldHost);
        auto* presetMenu = dynamic_cast<juce::ComboBox*> (find (*editor, "presets"));
        bool saveLabelFound = false;
        for (juce::PopupMenu::MenuItemIterator item (*presetMenu->getRootMenu()); item.next();)
            if (item.getItem().itemID == 20000) saveLabelFound = item.getItem().text == "Save preset...";
        require (saveLabelFound, "save-preset menu label is corrupted");

        juce::AudioBuffer<float> block (2, 400);
        juce::MidiBuffer midi;
        const auto processSilence = [&]
        {
            block.clear();
            processor.processBlock (block, midi);
            editor->refreshDisplay();
        };
        const auto value = [&] (const char* id) { return processor.parameters.getRawParameterValue (id)->load(); };
        processSilence();
        require (processor.isReceivingHostTempo() && std::abs (processor.getEffectiveBpm() - 96.0) < .01, "host tempo was not applied");
        auto* time = dynamic_cast<juce::Slider*> (find (*editor, "time"));
        require (time != nullptr, "Time knob missing");
        time->setValue (2.0, juce::sendNotificationSync);
        require (std::abs (value (motefield::parameter::division) - 2.0f) < .01f, "synced Time did not change subdivision");
        click (*editor, "sync");
        time->setValue (133.0, juce::sendNotificationSync);
        processSilence();
        require (std::abs (value (motefield::parameter::tempo) - 133.0f) < .01f, "manual Time did not change tempo");
        require (! processor.isReceivingHostTempo() && std::abs (processor.getEffectiveBpm() - 133.0) < .01, "manual tempo was overwritten by host");
        for (int i = 0; i < 11; ++i)
        {
            click (*editor, "mode-" + juce::String (i));
            require (std::abs (value (motefield::parameter::mode) - static_cast<float> (i)) < .01f, "bank selection did not reach processor");
        }
        auto* modeDial = dynamic_cast<juce::Slider*> (find (*editor, "mode-selector"));
        require (modeDial != nullptr, "hardware effect selector is missing");
        for (int i = 0; i < 11; ++i)
        {
            modeDial->setValue (static_cast<double> (i), juce::sendNotificationSync);
            require (std::abs (value (motefield::parameter::mode) - static_cast<float> (i)) < .01f, "mode dial did not reach host parameter");
            auto* label = find (*editor, "mode-" + juce::String (i));
            const auto delta = label->getBounds().getCentre().toFloat() - modeDial->getBounds().getCentre().toFloat();
            const auto angle = (-150.f + static_cast<float> (i) * 30.f) * juce::MathConstants<float>::pi / 180.f;
            const auto alignment = (delta.x * std::sin (angle) - delta.y * std::cos (angle)) / delta.getDistanceFromOrigin();
            require (alignment > .999f, "mode pointer is not aligned with its effect label");
            for (int j = i + 1; j < 11; ++j)
                require (! label->getBounds().intersects (find (*editor, "mode-" + juce::String (j))->getBounds()), "effect label hit areas overlap");
        }
        for (int i = 0; i < 4; ++i)
        {
            click (*editor, "variation-" + juce::String (i));
            require (std::abs (value (motefield::parameter::variation) - static_cast<float> (i)) < .01f, "variation did not reach processor");
        }
        click (*editor, "freeze");
        click (*editor, "bypass");
        require (value (motefield::parameter::freeze) > .5f && value (motefield::parameter::bypass) > .5f, "performance pad attachment failed");
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        processor.setParameterValue (motefield::parameter::shape, .1f);
        processor.setParameterValue (motefield::parameter::tempo, 60.0f);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        require (std::abs (value (motefield::parameter::tempo) - 133.0f) < .01f, "parameter state restore failed");
        auto oldState = processor.parameters.copyState();
        for (int i = oldState.getNumChildren() - 1; i >= 0; --i)
            if (oldState.getChild (i).getProperty ("id").toString() == motefield::parameter::bypass)
                oldState.removeChild (i, nullptr);
        juce::MemoryBlock oldBytes;
        juce::AudioProcessor::copyXmlToBinary (*oldState.createXml(), oldBytes);
        processor.setStateInformation (oldBytes.getData(), static_cast<int> (oldBytes.getSize()));
        require (value (motefield::parameter::bypass) < .5f, "v0.1 state did not clear a newer bypass setting");
        processor.setParameterValue (motefield::parameter::freeze, 0.0f);
        processor.setParameterValue (motefield::parameter::bypass, 0.0f);
        click (*editor, "record"); processSilence();
        require (processor.getLooperState() == motefield::LooperState::recording, "record control failed");
        click (*editor, "play"); processSilence();
        require (processor.getLooperState() == motefield::LooperState::playing, "play control failed");
        click (*editor, "dub"); processSilence();
        require (processor.getLooperState() == motefield::LooperState::overdubbing, "dub control failed");
        click (*editor, "stop"); processSilence();
        require (processor.getLooperState() == motefield::LooperState::stopped, "stop control failed");
        click (*editor, "erase"); processSilence();
        require (processor.getLooperState() == motefield::LooperState::empty, "erase control failed");
        checkPresetsAndAutomation (processor, destination);
        checkMaterialResponse (destination);
        auto* presets = dynamic_cast<juce::ComboBox*> (find (*editor, "presets"));
        require (presets != nullptr, "preset selector missing");
        presets->setSelectedId (1, juce::sendNotificationSync);
        pump();
        processor.setParameterValue (motefield::parameter::sync, 1.0f);
        processSilence();
        editor->refreshDisplay();
        saveImage (*editor, destination.getChildFile ("motefield-idle.png"));
        const auto closedSize = editor->getLocalBounds();
        click (*editor, "details");
        require (editor->getHeight() <= closedSize.getHeight() && editor->getWidth() <= closedSize.getWidth(), "Details expanded beyond the existing window");
        require (std::abs (static_cast<double> (editor->getWidth()) / editor->getHeight() - 1240.0 / 990.0) < .003, "Details aspect ratio is incorrect");
        auto* outputKnob = find (*editor, motefield::parameter::output);
        require (outputKnob != nullptr && outputKnob->isVisible() && outputKnob->getParentComponent()->isVisible(), "Details drawer did not open");
        require (editor->getLocalBounds().contains (editor->getLocalArea (outputKnob, outputKnob->getLocalBounds())), "Details output knob is clipped");
        for (auto* child : editor->getChildren())
            if (child->isVisible()) require (editor->getLocalBounds().contains (child->getBounds()), "Details contains a clipped control");
        saveImage (*editor, destination.getChildFile ("motefield-details.png"));
        click (*editor, "details");
        editor->setSize (1000, 645);
        saveImage (*editor, destination.getChildFile ("motefield-small.png"));
        click (*editor,"perform");
        auto* panel = find (*editor,"performance-panel"); require (panel != nullptr && panel->isVisible(),"performance panel did not open");
        require (editor->getLocalBounds().contains (panel->getBounds()),"performance panel exceeds window");
        auto* pages = dynamic_cast<juce::ComboBox*> (find (*editor,"perform-pages")); require (pages != nullptr,"performance pages missing");
        for (int page = 1; page <= 3; ++page)
        { pages->setSelectedId (page,juce::sendNotificationSync); saveImage (*editor,destination.getChildFile ("motefield-perform-" + juce::String (page) + ".png")); }
        click (*editor,"perform-close"); require (! panel->isVisible(),"performance panel did not close");
        editor->setSize (1240, 800);
        std::cout << "UI checks passed: 11 modes, 4 variations, synced/manual Time, performance pads, state restore, looper controls, Details and resizing.\n";
        if (argc > 2 && juce::String (argv[2]) == "--stills-only")
        {
            for (const auto mode : { 6, 4 })
            {
                processor.reset();
                processor.setParameterValue (motefield::parameter::mode, static_cast<float> (mode));
                processor.setParameterValue (motefield::parameter::density, .7f);
                for (int chunk = 0; chunk < 240; ++chunk)
                {
                    for (int sample = 0; sample < block.getNumSamples(); ++sample)
                    {
                        const auto input = pluck (chunk * block.getNumSamples() + sample);
                        block.setSample (0, sample, input);
                        block.setSample (1, sample, input * .93f);
                    }
                    processor.processBlock (block, midi);
                    editor->refreshDisplay();
                }
                saveImage (*editor, destination.getChildFile (mode == 6 ? "motefield-chop.png" : "motefield-orbit.png"));
            }
            click (*editor, motefield::parameter::reverse);
            require (value (motefield::parameter::reverse) > .5f, "effect Reverse attachment failed");
            saveImage (*editor, destination.getChildFile ("motefield-reverse.png"));
            processor.setParameterValue (motefield::parameter::shape, 0.0f);
            editor->refreshDisplay();
            saveImage (*editor, destination.getChildFile ("motefield-shape-soft.png"));
            processor.setParameterValue (motefield::parameter::shape, 1.0f);
            editor->refreshDisplay();
            saveImage (*editor, destination.getChildFile ("motefield-shape-tight.png"));
            return 0;
        }

        // Render the actual processor and editor, with a synthesized plucked input.
        // The raw video is a reproducible temporary file for ffmpeg, not product code.
        processor.reset();
        processor.prepareToPlay (48000.0, 400);
        processor.applyFactoryPreset (0);
        processor.setParameterValue (motefield::parameter::looperLevel, .35f);
        juce::AudioBuffer<float> audio (2, 48000 * 12);
        auto raw = destination.getChildFile ("motefield-preview.bgr").createOutputStream();
        require (raw != nullptr, "cannot create raw preview");
        require (raw->setPosition (0) && raw->truncate().wasOk(), "cannot reset preview file");
        std::vector<juce::uint8> bgrRow (static_cast<std::size_t> (editor->getWidth() * 3));
        for (int frame = 0; frame < 360; ++frame)
        {
            if (frame == 0) processor.requestLooperCommand (motefield::LooperCommand::record);
            if (frame == 48) processor.requestLooperCommand (motefield::LooperCommand::play);
            if (frame == 75) processor.requestLooperCommand (motefield::LooperCommand::stop);
            if (frame == 90) processor.setParameterValue (motefield::parameter::freeze, 1.0f);
            if (frame == 120) { processor.setParameterValue (motefield::parameter::freeze, 0.0f); processor.applyFactoryPreset (1); }
            if (frame == 180) processor.setParameterValue (motefield::parameter::reverse, 1.0f);
            if (frame == 210) processor.applyFactoryPreset (4);
            if (frame == 270) processor.applyFactoryPreset (5);
            if (frame == 330) processor.setParameterValue (motefield::parameter::freeze, 1.0f);
            for (int chunk = 0; chunk < 4; ++chunk)
            {
                const auto offset = frame * 1600 + chunk * 400;
                for (int sample = 0; sample < 400; ++sample)
                {
                    const auto input = (frame >= 90 && frame < 120) || frame >= 330 ? 0.0f : pluck (offset + sample);
                    block.setSample (0, sample, input);
                    block.setSample (1, sample, input * .93f);
                }
                processor.processBlock (block, midi);
                audio.copyFrom (0, offset, block, 0, 0, 400);
                audio.copyFrom (1, offset, block, 1, 0, 400);
            }
            editor->refreshDisplay();
            auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds()).convertedToFormat (juce::Image::RGB);
            const juce::Image::BitmapData pixels (snapshot, juce::Image::BitmapData::readOnly);
            for (int row = 0; row < snapshot.getHeight(); ++row)
            {
                const auto* source = pixels.getLinePointer (row);
                for (int column = 0; column < snapshot.getWidth(); ++column)
                {
                    const auto offset = static_cast<std::size_t> (column * 3);
                    const auto* pixel = source + column * pixels.pixelStride;
                    bgrRow[offset] = pixel[juce::PixelRGB::indexB];
                    bgrRow[offset + 1] = pixel[juce::PixelRGB::indexG];
                    bgrRow[offset + 2] = pixel[juce::PixelRGB::indexR];
                }
                require (raw->write (bgrRow.data(), bgrRow.size()), "video write failed");
            }
            if (frame == 81) saveImage (*editor, destination.getChildFile ("motefield-ui-compiled.png"));
            if (frame == 108) saveImage (*editor, destination.getChildFile ("motefield-held.png"));
            if (frame == 160) saveImage (*editor, destination.getChildFile ("motefield-granules.png"));
            if (frame == 305) saveImage (*editor, destination.getChildFile ("motefield-delay.png"));
            if (frame % 90 == 0) std::cout << "Rendered " << frame << "/360 audio-driven UI frames\n" << std::flush;
        }
        raw.reset();
        auto wavFile = destination.getChildFile ("motefield-demo.wav").createOutputStream();
        require (wavFile != nullptr && wavFile->setPosition (0) && wavFile->truncate().wasOk(), "cannot reset WAV file");
        std::unique_ptr<juce::OutputStream> wavStream = std::move (wavFile);
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (wavStream, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (24));
        require (writer != nullptr, "cannot create demo WAV");
        require (writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()), "cannot write demo audio");
        std::cout << "Compiled UI preview and synthesized audio demo complete.\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
