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

void checkAppearance (MoteFieldAudioProcessorEditor& editor, const juce::File& destination)
{
    const auto acid = juce::Colour (appearance::acid);
    require (appearance::read(editor).cyan == acid, "Acid is not the default accent");
    MoteFieldAudioProcessor secondProcessor;
    std::unique_ptr<MoteFieldAudioProcessorEditor> second (static_cast<MoteFieldAudioProcessorEditor*>(secondProcessor.createEditor()));
    click (editor,"settings");
    auto* hex=dynamic_cast<juce::TextEditor*>(find(editor,"accent-hex"));
    require (hex!=nullptr,"Settings has no custom accent input");
    hex->setText ("#123");hex->onReturnKey();
    require (appearance::read(editor).cyan==acid,"invalid hex changed appearance");
    hex->setText ("#67CAED");hex->onReturnKey();
    click (editor,"dark-mode");second->refreshDisplay();
    require (appearance::read(editor).cyan==juce::Colour(0xff67caed),"custom accent did not apply");
    require (appearance::read(second.operator*()).cyan==juce::Colour(0xff67caed),"open instances did not receive appearance");
    require (appearance::read(editor).paper.getBrightness()<.5f,"dark mode did not apply");
    saveImage (editor,destination.getChildFile("motefield-settings.png"));
    click (editor,"settings-close");
    require (!find(editor,"settings-panel")->isVisible(),"Settings did not close");
    { appearance::Preferences restored;
      require (restored.dark && restored.accent==juce::Colour(0xff67caed),"appearance did not persist to disk"); }
    saveImage (editor,destination.getChildFile("motefield-dark-custom.png"));
    click(editor,"settings");click(editor,"appearance-reset");click(editor,"settings-close");
    saveImage (editor,destination.getChildFile("motefield-dark-acid.png"));
    click(editor,"settings");click(editor,"dark-mode");click(editor,"settings-close");
    require (appearance::read(editor).cyan==acid,"Acid reset failed");
    for (const auto size : {juce::Point<int>(640,384),{1000,600},{1620,972}})
    {
        editor.setSize(size.x,size.y);click(editor,"settings");
        auto* panel=find(editor,"settings-panel");
        for (auto* child:panel->getChildren()) require (panel->getLocalBounds().contains(child->getBounds()),"Settings control clipped at small size");
        click(editor,"settings-close");
    }
    editor.setSize(1000,600);
    std::cout << "Appearance checks passed: Acid default, custom hex, invalid input, dark mode, open instances, disk persistence, reset and small windows.\n";
}

void checkMaterialResponse (const juce::File& destination)
{
    FieldDisplay display;display.setSize (718, 198);display.setSampleRate (48000.0);
    motefield::VisualFrame frame;display.update (frame);
    const auto snapshot = [&] { return display.createComponentSnapshot (display.getLocalBounds()); };
    const auto rest = snapshot();
    juce::Rectangle<int> restingBounds;
    for(int y=0;y<rest.getHeight();++y)for(int x=0;x<480;++x)
        if(rest.getPixelAt(x,y).getAlpha()>128) restingBounds=restingBounds.getUnion({x,y,1,1});
    require(restingBounds.getWidth()>60 && std::abs(restingBounds.getWidth()-restingBounds.getHeight())<5,
            "resting reactor must be a round fluid volume, not a logo silhouette");
    const auto changed = [] (const juce::Image& a, const juce::Image& b)
    {
        int result = 0;
        for (int y = 47; y < 177; ++y) for (int x = 28; x < 480; ++x)
            if (a.getPixelAt (x, y) != b.getPixelAt (x, y)) ++result;
        return result;
    };
    int lastMotionStep = 0;
    std::array<juce::Image, 3> bands;
    for (std::size_t band = 0; band < bands.size(); ++band)
    {
        frame.spectralEnergy = {};frame.spectralEnergy[band] = .16f;frame.outputLevel = .2f;
        for (int i = 0; i < 120; ++i) { const auto prior = i == 119 ? snapshot() : juce::Image {}; frame.sampleTime += 800;display.update (frame); if(i == 119)lastMotionStep=changed(prior,snapshot()); }
        bands[band] = snapshot();
        require (changed (rest, bands[band]) > 400, "main material did not react to audio band");
        const auto name = juce::StringArray { "bass", "mid", "treble" }[static_cast<int> (band)];
        auto stream = destination.getChildFile ("material-" + name + ".png").createOutputStream();
        require (stream != nullptr && stream->setPosition (0) && stream->truncate().wasOk(), "material screenshot could not be written");
        require (juce::PNGImageFormat().writeImageToStream (bands[band], *stream), "material screenshot failed");
    }
    require (changed (bands[0], bands[1]) > 400 && changed (bands[1], bands[2]) > 400, "audio bands produce the same material response");
    for (int i = 0; i < 3; ++i) display.update (frame);
    require (changed (bands[2], snapshot()) < lastMotionStep * 4 + 100, "material jumped beyond its ongoing motion between audio snapshots");
    frame.outputLevel = 0.f;frame.spectralEnergy = {};
    for (int i = 0; i < 300; ++i) { frame.sampleTime += 800;display.update (frame); }
    require (changed (rest, snapshot()) == 0, "material did not return to its resting surface");
    frame.mode=motefield::Mode::orbit;frame.voiceCount=2;
    frame.voices[0].id=1;frame.voices[0].level=.3f;frame.voices[0].envelope=1.f;frame.voices[0].phase=.05f;
    frame.voices[1]=frame.voices[0];frame.voices[1].id=2;frame.voices[1].phase=.55f;
    for(int i=0;i<120;++i){frame.sampleTime+=800;display.update(frame);}
    const auto voicesFirst=snapshot();
    frame.voices[0].phase=.3f;frame.voices[1].phase=.8f;
    for(int i=0;i<120;++i){frame.sampleTime+=800;display.update(frame);}
    require(changed(voicesFirst,snapshot())>800,"voice motion did not reshape the reactor with unchanged meters");
    std::cout << "Material checks passed: distinct bass/mid/treble deformation, snapshot-gap stability and silence settling.\n";
}

void checkInitialization (MoteFieldAudioProcessor& processor, MoteFieldAudioProcessorEditor& editor, const juce::File& destination)
{
    auto* menu = dynamic_cast<juce::ComboBox*> (find (editor, "presets"));
    require (menu != nullptr, "initialization preset menu missing");
    const juce::StringArray retained { "mode", "division", "tempo", "sync", "output", "freeze", "bypass",
        "looperLevel", "looperSpeed", "looperReverse", "looperOrder", "loopQuantize", "loopContinuous",
        "loopRate", "loopFade", "loopFadeMode", "loopOnly", "loopRecordOrder", "burstGate",
        "bypassTrails", "holdStyle", "wetSolo", "levelMatch", "loopRecordStart", "loopCountIn", "loopLength" };
    const auto phrase = processor.loopData();
    require (phrase.getSize() > 17, "initialization test requires recorded audio");
    const auto transport = processor.getLooperState();
    const auto folder = destination.getNonexistentChildFile ("initialize-checks", {}, false);
    require (processor.saveUserPreset ("Keep me", false, folder).wasOk(), "init fixture save failed");
    const auto originalFile = folder.getChildFile ("Keep me.motefield").loadFileAsString();
    for (int mode = 0; mode < 11; ++mode)
    {
        processor.setParameterValue ("mode", static_cast<float> (mode));
        processor.setParameterValue ("variation", 3.f);
        processor.setParameterValue ("repeats", .85f);
        processor.setParameterValue ("modDepth", .7f);
        processor.setParameterValue ("fieldPitch", 12.f);
        processor.setParameterValue ("width", 1.8f);
        processor.setParameterValue ("output", -3.f);
        processor.setParameterValue ("tempo", 137.f);
        std::vector<std::pair<juce::String, float>> before;
        for (auto* parameter : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                before.emplace_back (ranged->paramID, ranged->getValue());
        const auto name = processor.currentPresetName();
        const bool modified = processor.isPresetModified();
        HostParameterObserver host;
        auto* shape = processor.parameters.getParameter ("shape");
        shape->addListener (&host);
        menu->setSelectedId (20002, juce::sendNotificationSync);
        shape->removeListener (&host);
        require (host.starts == 1 && host.ends == 1, "initialization did not send balanced host gestures");
        const auto value = [&] (const char* id) { return processor.parameters.getRawParameterValue (id)->load(); };
        require (value ("mode") == mode && value ("variation") == 0.f, "initialization changed mode or failed to select A");
        require (std::abs(value ("width") - 1.f) < .001f && value ("mix") > .3f && value ("repeats") <= .36f
            && value ("space") == 0.f && value ("modDepth") == 0.f && std::abs(value ("fieldPitch")) < .001f,
            "initialization did not simplify the sound");
        for (const auto& [id, previous] : before)
            if (retained.contains (id) || id.startsWith ("loopRecordTrigger") || id.endsWith ("Trigger"))
                require (processor.parameters.getParameter (id)->getValue() == previous, "initialization changed a protected control");
        require (processor.currentPresetName() == juce::String ("Init - ") + motefield::modeNames[static_cast<std::size_t> (mode)]
            && ! processor.isPresetModified(), "initialized name/baseline incorrect");
        require (processor.loopData() == phrase && processor.getLooperState() == transport, "initialization changed recorded audio or transport");
        processor.setParameterValue ("output", -6.f);
        menu->setSelectedId (20003, juce::sendNotificationSync);
        for (const auto& [id, previous] : before)
            if (id != "output") require (std::abs (processor.parameters.getParameter (id)->getValue() - previous) < .00001f, "initialization undo did not restore sound");
        require (value ("output") == -6.f, "initialization undo reverted a later output change");
        require (! processor.canUndoInitialization() && processor.currentPresetName() == name
            && processor.isPresetModified() == modified, "initialization undo lost preset metadata");
    }
    require (folder.getChildFile ("Keep me.motefield").loadFileAsString() == originalFile, "initialization overwrote a saved preset");
    processor.initializeSound();
    require (processor.saveUserPreset ("My init", false, folder).wasOk(), "initialized sound save failed");
    require (! processor.canUndoInitialization(), "saving left stale initialization undo");
    processor.randomizeSound (123);
    require (processor.loadUserPreset (folder.getChildFile ("My init.motefield")).wasOk(), "initialized sound recall failed");
    require (processor.parameters.getRawParameterValue ("mode")->load() == 10.f
        && processor.parameters.getRawParameterValue ("modDepth")->load() == 0.f, "initialized preset did not recall");
    processor.initializeSound(); processor.applyFactoryPreset (17);
    require (! processor.canUndoInitialization(), "factory preset left stale initialization undo");
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary (*processor.parameters.copyState().createXml(), state);
    processor.initializeSound(); processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    require (! processor.canUndoInitialization(), "session restore left stale initialization undo");
    std::cout << "Initialization checks passed: all 11 modes, menu actions, balanced host gestures, protected controls/loop, exact sound undo, preset identity, disk preservation and save/recall.\n";
}

void checkPresetsAndAutomation (MoteFieldAudioProcessor& processor, const juce::File& destination)
{
    using namespace motefield::parameter;
    const auto directory = destination.getNonexistentChildFile ("preset-checks", {}, false);
    const auto names = MoteFieldAudioProcessor::factoryPresetNames();
    require (names.size() == 37, "factory preset count differs from the bank");
    std::set<int> modes;
    for (int i = 0; i < names.size(); ++i)
    {
        processor.setParameterValue ("fieldPitch", -16.25f); processor.setParameterValue ("tuningReference", 432);
        processor.setParameterValue ("fieldPosition", .85f); processor.setParameterValue ("fieldSplit", .9f);
        processor.applyFactoryPreset (i);
        const float pitches[] { 7,12,-12,3,.06f };
        require (std::abs (processor.parameters.getRawParameterValue ("fieldPitch")->load() - (i < 32 ? 0.f : pitches[i-32])) < .011f, "factory pitch leaked between patches");
        require (processor.parameters.getRawParameterValue ("tuningReference")->load() == 440.f, "factory reference is not 440 Hz");
        require (processor.parameters.getRawParameterValue ("fieldPosition")->load() == 0.f && processor.parameters.getRawParameterValue ("fieldSplit")->load() == 0.f, "liquid gesture leaked between patches");
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
    processor.setParameterValue ("width", 1.63f);
    processor.setParameterValue ("fieldPitch", 7.06f); processor.setParameterValue ("tuningReference", 442.f);
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
    require (std::abs(processor.parameters.getRawParameterValue("width")->load()-1.63f)<.001f,"width did not round trip in preset");
    require (std::abs(processor.parameters.getRawParameterValue("fieldPitch")->load()-7.06f)<.011f && processor.parameters.getRawParameterValue("tuningReference")->load()==442.f,"user tuning did not round trip");
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
    auto oldPreset = juce::XmlDocument::parse (file);
    require (oldPreset != nullptr, "legacy preset fixture failed");
    for (auto* child = oldPreset->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (juce::StringArray {"tuningReference","loopRecordStart","loopCountIn","loopLength"}.contains(child->getStringAttribute("id"))) oldPreset->removeChildElement(child,true);
        child = next;
    }
    auto legacyFile=directory.getChildFile("legacy-036.motefield"); oldPreset->writeTo(legacyFile);
    require(processor.loadUserPreset(legacyFile).wasOk(),"0.3.6 user preset rejected");
    require(processor.parameters.getRawParameterValue("tuningReference")->load()==440.f && std::abs(processor.parameters.getRawParameterValue("fieldPitch")->load()-7.06f)<.011f,"legacy preset lost pitch or inherited reference");


    require (processor.getParameters().size() == 65, "host parameter count changed unexpectedly");
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
    std::cout << "Preset/automation checks passed: 37 factory presets, all 11 modes, disk round trip, overwrite protection, invalid-file rejection, session identity, 65 host parameters and looper triggers.\n";
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
    for (const auto main : {juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()})
        for (const auto auxiliary : {juce::AudioChannelSet::disabled(),juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()})
        {
            MoteFieldAudioProcessor instance; auto layout=instance.getBusesLayout();
            layout.inputBuses.set(0,main);layout.outputBuses.set(0,main);layout.inputBuses.set(1,auxiliary);
            require(instance.setBusesLayout(layout),"main/auxiliary mono-stereo negotiation failed");
            instance.prepareToPlay(48000,128);instance.setParameterValue("magnetAmount",1.f);
            juce::AudioBuffer<float> input(main.size()+auxiliary.size(),400);input.clear();
            // Oversized block also exercises both auxiliary pointer offsets.
            if(auxiliary.size()>0)for(int i=0;i<400;++i)input.setSample(main.size()+auxiliary.size()-1,i,.5f);
            for(int tick=0;tick<4;++tick)instance.processBlock(input,midi);
            motefield::VisualFrame telemetry;instance.readVisualFrame(telemetry);
            if(auxiliary.size()>0)require(telemetry.magnet>.01f,"right-only sidechain was lost");
            for(int ch=0;ch<main.size();++ch)for(int i=0;i<400;++i)require(std::isfinite(input.getSample(ch,i)),"non-finite layout output");
        }
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
        const auto appearanceFile=destination.getNonexistentChildFile ("appearance-test", ".settings");
        #if JUCE_WINDOWS
        _putenv_s ("MOTEFIELD_APPEARANCE_FILE", appearanceFile.getFullPathName().toRawUTF8());
        #else
        setenv ("MOTEFIELD_APPEARANCE_FILE", appearanceFile.getFullPathName().toRawUTF8(),1);
        #endif
        MoteFieldAudioProcessor processor;
        if (argc > 2 && juce::String (argv[2]) == "--appearance-only")
        {
            processor.prepareToPlay(48000,400); processor.applyFactoryPreset(17);
            PlayHead head;processor.setPlayHead(&head);
            std::unique_ptr<MoteFieldAudioProcessorEditor> editor(static_cast<MoteFieldAudioProcessorEditor*>(processor.createEditor()));
            editor->setVisible(true);editor->setSize(1620,972);
            juce::AudioBuffer<float> block(2,400);juce::MidiBuffer midi;
            processor.requestLooperCommand(motefield::LooperCommand::record);
            for(int chunk=0;chunk<240;++chunk)
            {
                for(int sample=0;sample<400;++sample) for(int channel=0;channel<2;++channel) block.setSample(channel,sample,pluck(chunk*400+sample));
                if(chunk==200) processor.requestLooperCommand(motefield::LooperCommand::play);
                processor.processBlock(block,midi);editor->refreshDisplay();
            }
            saveImage(*editor,destination.getChildFile("motefield-light-acid.png"));
            checkAppearance(*editor,destination);
            return 0;
        }
        if (argc > 2 && juce::String (argv[2]) == "--print-only")
        {
            processor.prepareToPlay (48000.0, 400); processor.applyFactoryPreset (17);
            PlayHead playhead; processor.setPlayHead (&playhead);
            std::unique_ptr<MoteFieldAudioProcessorEditor> editor (static_cast<MoteFieldAudioProcessorEditor*> (processor.createEditor()));
            editor->setSize (1364, 880); editor->setVisible (true);
            editor->getProperties().set ("facePreview", 1);
            juce::AudioBuffer<float> block (2, 400); juce::MidiBuffer midi;
            for (int chunk = 0; chunk < 240; ++chunk)
            {
                for (int i = 0; i < 400; ++i) for (int c = 0; c < 2; ++c) block.setSample (c, i, pluck (chunk * 400 + i));
                processor.processBlock (block, midi); editor->refreshDisplay();
            }
            juce::Image comparison (juce::Image::RGB, 1920, 1060, true);
            juce::Graphics g (comparison); g.fillAll (juce::Colour (0xffe8e2d0));
            g.setColour (juce::Colour (0xff22261f)); g.setFont (juce::FontOptions (34.f));
            g.drawText ("MoteField / print studies", 36, 22, 1500, 55, juce::Justification::centredLeft);
            auto names = juce::StringArray::fromLines (destination.getChildFile ("titles.txt").loadFileAsString());
            names.removeEmptyStrings();
            if (names.size() != 3) names = { "01 / ORGANIC SCATTER", "02 / LIQUID RIBBONS", "03 / BOLD HIDE" };
            auto captions = juce::StringArray::fromLines (destination.getChildFile ("captions.txt").loadFileAsString());
            captions.removeEmptyStrings();
            if (captions.size() != 3) captions = { "Separate, uneven islands of ink.", "Longer shapes connect the control groups.", "Larger patches and stronger contrast." };
            for (int option = 0; option < 3; ++option)
            {
                const auto svg = destination.getChildFile ("print-0" + juce::String (option + 1) + ".svg").loadFileAsString();
                require (svg.isNotEmpty(), "missing print source");
                editor->getProperties().set ("printPreviewSVG", svg); editor->resized();
                saveImage (*editor, destination.getChildFile ("print-0" + juce::String (option + 1) + ".png"));
                const auto shot = editor->createComponentSnapshot (editor->getLocalBounds());
                const int x = 36 + option * 630;
                g.setColour (juce::Colour (0xff22261f)); g.setFont (juce::FontOptions (22.f, juce::Font::bold));
                g.drawText (names[option], x, 111, 590, 35, juce::Justification::centredLeft);
                g.drawImageWithin (shot, x, 168, 588, 380, juce::RectanglePlacement::centred);
                const auto detail = shot.getClippedImage ({45,110,810,399}).createCopy();
                g.drawImageWithin (detail, x, 590, 588, 290, juce::RectanglePlacement::centred);
                g.setFont (juce::FontOptions (16.f));
                g.drawText (captions[option], x, 907, 590, 50, juce::Justification::centredLeft);
            }
            auto stream = destination.getChildFile ("MoteField-Print-Options.png").createOutputStream();
            require (stream != nullptr, "cannot create print comparison"); stream->setPosition (0); stream->truncate();
            require (juce::PNGImageFormat().writeImageToStream (comparison,*stream), "print comparison failed");
            std::cout << "Three native print studies captured.\n"; return 0;
        }
        if (argc > 2 && juce::String (argv[2]) == "--faces-only")
        {
            processor.prepareToPlay (48000.0, 400); processor.applyFactoryPreset (17);
            PlayHead playhead; processor.setPlayHead (&playhead);
            std::unique_ptr<MoteFieldAudioProcessorEditor> editor (static_cast<MoteFieldAudioProcessorEditor*> (processor.createEditor()));
            editor->setSize (1364, 880); editor->setVisible (true);
            juce::AudioBuffer<float> block (2, 400); juce::MidiBuffer midi;
            for (int chunk = 0; chunk < 240; ++chunk)
            {
                for (int i = 0; i < 400; ++i) for (int channel = 0; channel < 2; ++channel) block.setSample (channel, i, pluck (chunk * 400 + i));
                processor.processBlock (block, midi); editor->refreshDisplay();
            }
            juce::Image comparison (juce::Image::RGB, 1920, 1040, true);
            juce::Graphics g (comparison); g.fillAll (juce::Colour (0xffe8e2d0));
            const juce::StringArray names { "01 / MILK GLASS", "02 / CLEAR WARM GLASS", "03 / SMOKED GLASS" };
            g.setColour (juce::Colour (0xff22261f)); g.setFont (juce::FontOptions (35.f));
            g.drawText ("MoteField / face studies", 48, 25, 1400, 54, juce::Justification::centredLeft);
            g.setFont (juce::FontOptions (18.f)); g.drawText ("Same instrument. Three glass finishes for the effect selector.", 48, 85, 1400, 30, juce::Justification::centredLeft);
            for (int finish = 0; finish < 3; ++finish)
            {
                editor->getProperties().set ("facePreview", finish); editor->resized();
                saveImage (*editor, destination.getChildFile ("face-0" + juce::String (finish + 1) + ".png"));
                const auto shot = editor->createComponentSnapshot (editor->getLocalBounds());
                const auto plate = shot.getClippedImage ({864,114,463,421}).createCopy();
                auto stream = destination.getChildFile ("panel-0" + juce::String (finish + 1) + ".png").createOutputStream();
                require (stream != nullptr, "cannot create face detail"); stream->setPosition (0); stream->truncate(); juce::PNGImageFormat().writeImageToStream (plate,*stream);
                const int x = 48 + finish * 624;
                g.setColour (juce::Colour (0xff22261f)); g.setFont (juce::FontOptions (21.f, juce::Font::bold));
                g.drawText (names[finish], x, 156, 580, 32, juce::Justification::centredLeft);
                g.drawImageWithin (plate, x, 213, 576, 526, juce::RectanglePlacement::centred);
                g.drawImageWithin (shot, x, 765, 370, 240, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
            }
            auto stream = destination.getChildFile ("MoteField-Face-Options.png").createOutputStream();
            require (stream != nullptr, "cannot create comparison"); stream->setPosition (0); stream->truncate();
            require (juce::PNGImageFormat().writeImageToStream (comparison,*stream), "face comparison failed");
            std::cout << "Three native face studies captured.\n"; return 0;
        }
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
            const auto angle = (-120.f + static_cast<float> (i) * 30.f) * juce::MathConstants<float>::pi / 180.f;
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
        click(*editor,"wetSolo"); click(*editor,"levelMatch");
        require(value("wetSolo")>.5f && value("levelMatch")>.5f,"monitor button attachment failed");
        require(!find(*editor,"wetSolo")->getBounds().intersects(find(*editor,"levelMatch")->getBounds()),"monitor buttons overlap");
        processor.setParameterValue("width",1.7f);
        auto oldState = processor.parameters.copyState();
        for (int i = oldState.getNumChildren() - 1; i >= 0; --i)
            if (juce::StringArray { motefield::parameter::bypass, "width", "wetSolo", "levelMatch", "tuningReference", "loopRecordStart", "loopCountIn", "loopLength" }.contains(oldState.getChild (i).getProperty ("id").toString()))
                oldState.removeChild (i, nullptr);
        juce::MemoryBlock oldBytes;
        juce::AudioProcessor::copyXmlToBinary (*oldState.createXml(), oldBytes);
        processor.setStateInformation (oldBytes.getData(), static_cast<int> (oldBytes.getSize()));
        require (value (motefield::parameter::bypass) < .5f, "v0.1 state did not clear a newer bypass setting");
        require(value("width")==1.f && value("wetSolo")==0.f && value("levelMatch")==0.f,"legacy session failed neutral monitor defaults");
        require(value("tuningReference")==440.f && value("loopRecordStart")==0.f && value("loopCountIn")==0.f && value("loopLength")==0.f,"legacy session failed new tuning/record defaults");
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
        {
            const std::set<juce::String> randomized { "mode","variation","density","repeats","shape","cutoff","mix","space","modDepth","modRate","resonance","division","reverbStyle","reverse" };
            std::vector<std::pair<juce::String,float>> protectedValues;
            for(auto* parameter:processor.getParameters())
                if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter);ranged && !randomized.count(ranged->paramID))
                    protectedValues.emplace_back(ranged->paramID,ranged->getValue());
            const auto phrase=processor.loopData();
            HostParameterObserver randomHost;auto* shapeParam=processor.parameters.getParameter(motefield::parameter::shape);
            shapeParam->addListener(&randomHost);click(*editor,"random-preset");shapeParam->removeListener(&randomHost);
            require(randomHost.starts==1 && randomHost.ends==1 && randomHost.values>0,"Random did not send host gestures");
            for(const auto& entry:protectedValues)
                require(processor.parameters.getParameter(entry.first)->getValue()==entry.second,"Random changed a protected performance parameter");
            require(processor.loopData()==phrase && processor.getLooperState()==motefield::LooperState::stopped,"Random changed recorded audio or transport");
            processor.randomizeSound(42);const auto firstShape=value(motefield::parameter::shape);
            processor.randomizeSound(99);require(value(motefield::parameter::shape)!=firstShape,"Random did not create a different sound");
            processor.randomizeSound(42);require(value(motefield::parameter::shape)==firstShape,"Random seed is not repeatable");
            require(value(motefield::parameter::repeats)<=.761f && value(motefield::parameter::cutoff)>=1800.f,"Random exceeded musical ranges");
            const auto folder=destination.getNonexistentChildFile("random-preset-check",{},false);
            require(processor.saveUserPreset("Random test",false,folder).wasOk(),"Random sound could not be saved");
            processor.randomizeSound(87);
            require(processor.loadUserPreset(folder.getChildFile("Random test.motefield")).wasOk() && std::abs(value(motefield::parameter::shape)-firstShape)<.0001f,"Random preset did not recall");
            std::cout<<"Random checks passed: new sounds, host gestures, preserved loop/performance, repeatable seeds and user save/recall.\n";
        }
        processSilence(); // Commit the preceding preset's pending loop restore.
        checkInitialization (processor, *editor, destination);
        click (*editor, "erase"); processSilence();
        require (processor.getLooperState() == motefield::LooperState::empty, "erase control failed");
        // Both pages use the same hardware area; opening never starts text editing.
        processor.applyFactoryPreset (32); editor->refreshDisplay();
        for (const int width : { 1000, 1620 })
        {
            editor->setSize (width, width * 972 / 1620);
            click (*editor, "tuning-open");
            auto* tuning = dynamic_cast<TuningPanel*> (find (*editor, "tuning-panel"));
            require (tuning && tuning->isOpen(), "tuning page did not open");
            auto* entry = dynamic_cast<juce::TextEditor*> (find (*editor,"tuning-transpose"));
            require (entry && !entry->hasKeyboardFocus(false), "opening tuning auto-focused numeric entry");
            for (auto* child : tuning->getChildren()) if (child->isVisible()) require (tuning->getLocalBounds().contains (child->getBounds()), "tuning control clipped");
            require (!find(*editor,"wetSolo")->isVisible(), "monitor was not replaced by tuning page");
            entry->setText ("-16", false); entry->onReturnKey(); editor->refreshDisplay();
            require (std::abs(value("fieldPitch")+16.f)<.011f,"typed transpose did not update audio parameter");
            saveImage (*editor, destination.getChildFile ("motefield-tuning-" + juce::String(width) + ".png"));
            tuning->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            require (!tuning->isOpen() && find(*editor,"wetSolo")->isVisible(), "Escape failed to restore envelope page");
            click(*editor,"tuning-reset"); require (std::abs(value("fieldPitch"))<.011f,"tuning reset failed");
        }
        std::cout << "Tuning checks passed: in-place page, no auto-focus, typed interval, reset, Escape and two window sizes.\n";
        checkAppearance (*editor,destination);
        checkPresetsAndAutomation (processor, destination);
        require (appearance::read(*editor).cyan==juce::Colour(appearance::acid),"sound preset reset appearance");
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
        require (std::abs (static_cast<double> (editor->getWidth()) / editor->getHeight() - 1620.0 / 1220.0) < .003, "Details aspect ratio is incorrect");
        auto* outputKnob = find (*editor, motefield::parameter::output);
        require (outputKnob != nullptr && outputKnob->isVisible() && outputKnob->getParentComponent()->isVisible(), "Details drawer did not open");
        require (editor->getLocalBounds().contains (editor->getLocalArea (outputKnob, outputKnob->getLocalBounds())), "Details output knob is clipped");
        for (auto* child : editor->getChildren())
            if (child->isVisible()) require (editor->getLocalBounds().contains (child->getBounds()), "Details contains a clipped control");
        saveImage (*editor, destination.getChildFile ("motefield-details.png"));
        click (*editor, "details");
        editor->setSize (1000, 600);
        saveImage (*editor, destination.getChildFile ("motefield-small.png"));
        click (*editor,"perform");
        auto* panel = find (*editor,"performance-panel"); require (panel != nullptr && panel->isVisible(),"performance panel did not open");
        require (editor->getLocalBounds().contains (panel->getBounds()),"performance panel exceeds window");
        auto* pages = dynamic_cast<juce::ComboBox*> (find (*editor,"perform-pages")); require (pages != nullptr,"performance pages missing");
        for (int page = 1; page <= 3; ++page)
        { pages->setSelectedId (page,juce::sendNotificationSync); saveImage (*editor,destination.getChildFile ("motefield-perform-" + juce::String (page) + ".png")); }
        click (*editor,"perform-close"); require (! panel->isVisible(),"performance panel did not close");
        editor->setSize (1620, 972);
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
