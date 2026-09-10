#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>

MoteFieldAudioProcessor::MoteFieldAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Magnet", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MoteFieldState", createParameterLayout())
{
    for (const auto* id : motefield::parameter::looperTriggers)
        parameters.addParameterListener (id, this);
    clearMidiMappings();
    for (auto* p : getParameters()) if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p)) midiTargets.push_back (ranged);
    for (std::size_t i = 0; i < midiTargets.size(); ++i)
    { if (midiTargets[i]->paramID == "freeze") midiMap[64].store (static_cast<int> (i));
      if (midiTargets[i]->paramID == "modDepth") midiMap[1].store (static_cast<int> (i));
      if (midiTargets[i]->paramID == "mix") midiMap[11].store (static_cast<int> (i)); }
    startTimerHz (30);
}

MoteFieldAudioProcessor::~MoteFieldAudioProcessor()
{
    stopTimer();
    for (const auto* id : motefield::parameter::looperTriggers)
        parameters.removeParameterListener (id, this);
}

void MoteFieldAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    auto saved = loopData();
    { std::lock_guard<std::mutex> lock (archiveMutex);
      engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels()); prepared = true; currentSampleRate = sampleRate; }
    if (saved.getSize() > 0) restoreLoopData (saved);
    pendingLooperTriggers.store (0);
}

void MoteFieldAudioProcessor::releaseResources()
{
    auto saved = loopData();
    std::lock_guard<std::mutex> lock (archiveMutex);
    pendingLoopData = saved; prepared = false;
}

bool MoteFieldAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output) return false;
    // AU hosts may negotiate the auxiliary input as stereo even when no external
    // source is selected. Accept its layout independently of the main bus.
    if (layouts.inputBuses.size() > 1)
    {
        const auto& side = layouts.inputBuses[1];
        if (!side.isDisabled() && side != juce::AudioChannelSet::mono() && side != juce::AudioChannelSet::stereo()) return false;
    }
    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void MoteFieldAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    int cursor = 0;
    for (const auto metadata : midi)
    {
        const auto eventSample = juce::jlimit (cursor, buffer.getNumSamples(), metadata.samplePosition);
        if (eventSample > cursor) processAudio (buffer, cursor, eventSample - cursor);
        cursor = eventSample;
        const auto message = metadata.getMessage();
        if (message.isProgramChange()) requestedProgram.store (juce::jlimit (0, 36, message.getProgramChangeNumber()));
        if (! message.isController()) continue;
        const auto cc = message.getControllerNumber(), value = message.getControllerValue();
        const auto learning = midiLearn.exchange (-1);
        if (learning >= 0 && learning < static_cast<int> (midiTargets.size())) { midiMap[cc].store (learning); lastLearned.store (cc); }
        const auto target = midiMap[cc].load();
        if (target >= 0 && target < static_cast<int> (midiTargets.size()))
        {
            auto* parameter = midiTargets[static_cast<std::size_t> (target)];
            bool trigger = false; for (const auto* id : motefield::parameter::looperTriggers) trigger = trigger || parameter->paramID == id;
            const bool toggleHold = parameter->paramID == "freeze" && parameters.getRawParameterValue ("holdStyle")->load() < .5f;
            if (trigger || toggleHold) { if (value >= 64 && previousCC[cc] < 64) parameter->setValueNotifyingHost (parameter->getValue() > .5f ? 0.f : 1.f); }
            else parameter->setValueNotifyingHost (value / 127.f);
        }
        previousCC[cc] = value;
    }
    if (cursor < buffer.getNumSamples()) processAudio (buffer, cursor, buffer.getNumSamples() - cursor);
    midi.clear();
}

void MoteFieldAudioProcessor::processAudio (juce::AudioBuffer<float>& buffer, int offset, int samples)
{
    juce::ScopedNoDenormals noDenormals;
    // Both host automation and UI actions publish bounded, allocation-free flags.
    // Only this audio thread writes to the DSP command queue.
    const auto commands = pendingLooperTriggers.exchange (0, std::memory_order_acq_rel);
    constexpr std::array actions { motefield::LooperCommand::record, motefield::LooperCommand::play,
        motefield::LooperCommand::dub, motefield::LooperCommand::stop,
        motefield::LooperCommand::undo, motefield::LooperCommand::clear,
        motefield::LooperCommand::recordPlayDub, motefield::LooperCommand::stopPlay };
    for (std::size_t i = 0; i < actions.size(); ++i)
        if ((commands & (1u << i)) != 0) engine.requestLooperCommand (actions[i]);
    const auto channels = juce::jlimit (1, 2, getMainBusNumInputChannels());

    const auto load = [this] (const char* id)
    {
        return parameters.getRawParameterValue (id)->load (std::memory_order_relaxed);
    };

    motefield::EngineParameters values;
    values.mode = static_cast<motefield::Mode> (juce::jlimit (
        0, static_cast<int> (motefield::Mode::count) - 1,
        static_cast<int> (std::lround (load (motefield::parameter::mode)))));
    values.variation = juce::jlimit (0, 3, static_cast<int> (std::lround (load (motefield::parameter::variation))));
    values.density = load (motefield::parameter::density);
    values.repeats = load (motefield::parameter::repeats);
    values.shape = load (motefield::parameter::shape);
    values.division = juce::jlimit (0, 8, static_cast<int> (std::lround (load (motefield::parameter::division))));
    values.bpm = load (motefield::parameter::tempo);
    values.reverse = load (motefield::parameter::reverse) > 0.5f;
    values.freeze = load (motefield::parameter::freeze) > 0.5f;
    values.modulationDepth = load (motefield::parameter::modDepth);
    values.modulationRateHz = load (motefield::parameter::modRate);
    values.cutoffHz = load (motefield::parameter::cutoff);
    values.resonance = load (motefield::parameter::resonance);
    values.space = load (motefield::parameter::space);
    values.reverbStyle = juce::jlimit (0, 3, static_cast<int> (std::lround (load (motefield::parameter::reverbStyle))));
    values.mix = load (motefield::parameter::mix);
    values.width = load ("width");
    values.reverbSolo = load ("reverbSolo") > .5f;
    values.wetSolo = load ("wetSolo") > .5f;
    values.levelMatch = load ("levelMatch") > .5f;
    values.outputGain = decibelsToGain (load (motefield::parameter::output));
    values.looperLevel = load (motefield::parameter::looperLevel);
    values.looperReverse = load (motefield::parameter::looperReverse) > 0.5f;

    constexpr std::array<float, 3> looperSpeeds { 0.5f, 1.0f, 2.0f };
    const auto speedIndex = juce::jlimit (0, 2, static_cast<int> (std::lround (load (motefield::parameter::looperSpeed))));
    values.looperSpeed = looperSpeeds[static_cast<std::size_t> (speedIndex)];
    values.looperBeforeEffect = load (motefield::parameter::looperOrder) > 0.5f;
    values.bypass = load (motefield::parameter::bypass) > 0.5f;

    values.quantize = load ("loopQuantize") > .5f;
    values.recordStart = static_cast<int> (load ("loopRecordStart"));
    values.recordCountIn = load ("loopCountIn") > .5f;
    const int lengthChoice = static_cast<int> (load ("loopLength"));
    values.recordBars = lengthChoice == 0 ? 0 : 1 << (lengthChoice - 1);
    values.looperOnly = load ("loopOnly") > .5f; values.trails = load ("bypassTrails") > .5f;
    values.loopFade = load ("loopFade"); values.fadeMode = static_cast<int> (load ("loopFadeMode"));
    values.recordIntoDub = load ("loopRecordOrder") > .5f;
    if (load ("loopContinuous") > .5f) values.looperSpeed = load ("loopRate");
    const auto burst = load ("burstGate") > .5f;
    if (burst != previousBurst) { engine.requestLooperCommand (burst ? motefield::LooperCommand::burstStart : motefield::LooperCommand::burstEnd); previousBurst = burst; }
    values.viscosity = load ("viscosity"); values.cohesion = load ("cohesion"); values.tension = load ("tension");
    values.fieldPosition = load ("fieldPosition"); values.fieldPitch = load ("fieldPitch") + 12.f * std::log2 (load ("tuningReference") / 440.f);
    values.fieldStretch = load ("fieldStretch"); values.fieldSplit = load ("fieldSplit");
    values.magnetAmount = load ("magnetAmount"); values.magnetMode = static_cast<int> (load ("magnetMode"));
    values.magnetAttack = load ("magnetAttack"); values.magnetRelease = load ("magnetRelease");
    values.seed = static_cast<int> (load ("patternSeed")); values.patternLock = load ("patternLock") > .5f;
    values.patternSteps = static_cast<int> (load ("patternSteps"));
    values.rhythmMutation = static_cast<int> (load ("rhythmMutation")); values.pitchMutation = static_cast<int> (load ("pitchMutation"));
    values.scale = static_cast<int> (load ("scale")); values.root = static_cast<int> (load ("scaleRoot")); values.sourceNote = static_cast<int> (load ("sourceNote"));
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto side = getBusBuffer (buffer, true, 1);
        if (side.getNumChannels() > 0) values.sidechain = side.getReadPointer (0) + offset;
        if (side.getNumChannels() > 1) values.sidechainRight = side.getReadPointer (1) + offset;
    }
    bool hostTempoFound = false;
    beatsPerBar.store (4.0);
    if (load (motefield::parameter::sync) > 0.5f)
    {
        if (auto* hostPlayHead = getPlayHead())
            if (const auto position = hostPlayHead->getPosition())
            {
                if (const auto ppq = position->getPpqPosition()) { values.hostPpq = *ppq; values.hostPositionValid = true; }
                values.hostPlaying = position->getIsPlaying();
                if (const auto signature = position->getTimeSignature()) beatsPerBar.store (signature->numerator * 4.0 / signature->denominator);
                if (const auto bar = position->getPpqPositionOfLastBarStart()) values.hostBarStart = *bar;
                if (const auto hostBpm = position->getBpm())
                {
                    values.bpm = *hostBpm;
                    hostTempoFound = true;
                }
            }
    }
    values.beatsPerBar = beatsPerBar.load();
    values.hostPpq += offset / (currentSampleRate * 60.0 / values.bpm);
    receivingHostTempo.store (hostTempoFound, std::memory_order_relaxed);
    effectiveBpm.store (values.bpm, std::memory_order_relaxed);

    std::array<const float*, 2> inputs {
        buffer.getReadPointer (0) + offset,
        (channels > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0)) + offset
    };
    std::array<float*, 2> outputs {
        buffer.getWritePointer (0) + offset,
        (channels > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0)) + offset
    };
    engine.process (inputs.data(), outputs.data(), channels, samples, values);
}

juce::AudioProcessorEditor* MoteFieldAudioProcessor::createEditor()
{
    return new MoteFieldAudioProcessorEditor (*this);
}

void MoteFieldAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
    {
        auto* audio = xml->createNewChildElement ("LoopAudio"); audio->addTextElement (loopData().toBase64Encoding());
        auto* mappings = xml->createNewChildElement ("MidiMappings");
        for (std::size_t cc = 0; cc < midiMap.size(); ++cc) if (midiMap[cc].load() >= 0)
        { auto* map = mappings->createNewChildElement ("CC"); map->setAttribute ("cc", static_cast<int> (cc)); map->setAttribute ("target", midiTargets[static_cast<std::size_t> (midiMap[cc].load())]->paramID); }
        copyXmlToBinary (*xml, destinationData);
    }
}

void MoteFieldAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (parameters.state.getType().toString())) return;
        initializationUndoAvailable.store (false);
        if (auto* audio = xml->getChildByName ("LoopAudio"))
        { juce::MemoryBlock bytes; if (! bytes.fromBase64Encoding (audio->getAllSubText()) || ! restoreLoopData (bytes)) return; xml->removeChildElement (audio, true); }
        if (auto* mappings = xml->getChildByName ("MidiMappings"))
        { clearMidiMappings(); for (const auto* map : mappings->getChildIterator())
            for (std::size_t i = 0; i < midiTargets.size(); ++i) if (midiTargets[i]->paramID == map->getStringAttribute ("target"))
            { const auto cc = map->getIntAttribute ("cc", -1); if (cc >= 0 && cc < 128) midiMap[static_cast<std::size_t> (cc)].store (static_cast<int> (i)); }
          xml->removeChildElement (mappings, true); }
        auto restored = juce::ValueTree::fromXml (*xml);
        for (const auto& id : extendedParameterIds())
        {
            bool found = false; for (const auto child : restored) found = found || child.getProperty ("id").toString() == id;
            if (! found) { juce::ValueTree child ("PARAM"); child.setProperty ("id", id, nullptr); auto* p = parameters.getParameter (id); child.setProperty ("value", p->convertFrom0to1 (p->getDefaultValue()), nullptr); restored.addChild (child, -1, nullptr); }
        }
        if (restored.isValid() && restored.hasType (parameters.state.getType()))
        {
            restoringState.store (true);
            // v0.1 had no bypass parameter. Loading it into an already bypassed
            // instance must restore the audible v0.1 behavior.
            bool hasBypass = false;
            for (const auto child : restored)
                hasBypass = hasBypass || child.getProperty ("id").toString() == motefield::parameter::bypass;
            if (! hasBypass)
                parameters.getParameter (motefield::parameter::bypass)->setValueNotifyingHost (0.0f);
            parameters.replaceState (restored);
            pendingLooperTriggers.store (0); requestedProgram.store (-1);
            previousBurst = parameters.getRawParameterValue ("burstGate")->load() > .5f;
            restoringState.store (false);
        }
    }
}

void MoteFieldAudioProcessor::requestLooperCommand (motefield::LooperCommand command) noexcept
{
    if (command == motefield::LooperCommand::recordPlayDub || command == motefield::LooperCommand::stopPlay)
    {
        pendingLooperTriggers.fetch_or (1u << (command == motefield::LooperCommand::recordPlayDub ? 6 : 7));
        return;
    }
    constexpr std::array actions { motefield::LooperCommand::record, motefield::LooperCommand::play,
        motefield::LooperCommand::dub, motefield::LooperCommand::stop,
        motefield::LooperCommand::undo, motefield::LooperCommand::clear };
    for (std::size_t i = 0; i < actions.size(); ++i)
        if (command == actions[i])
        {
            const auto* id = motefield::parameter::looperTriggers[i];
            setParameterValue (id, parameters.getRawParameterValue (id)->load() > .5f ? 0.f : 1.f);
            return;
        }
}

void MoteFieldAudioProcessor::parameterChanged (const juce::String& id, float value)
{
    for (std::size_t i = 0; i < motefield::parameter::looperTriggers.size(); ++i)
        if (id == motefield::parameter::looperTriggers[i])
        {
            const bool next = value > .5f;
            const bool changed = previousLooperTriggers[i].exchange (next) != next;
            if (changed && ! restoringState.load()) pendingLooperTriggers.fetch_or (1u << i);
            return;
        }
}

motefield::LooperState MoteFieldAudioProcessor::getLooperState() const noexcept
{
    return engine.getLooperState();
}

float MoteFieldAudioProcessor::getLooperProgress() const noexcept
{
    return engine.getLooperProgress();
}

float MoteFieldAudioProcessor::getInputLevel() const noexcept
{
    return engine.getInputLevel();
}

float MoteFieldAudioProcessor::getEffectLevel() const noexcept
{
    return engine.getEffectLevel();
}

int MoteFieldAudioProcessor::getActiveGrainCount() const noexcept
{
    return engine.getActiveGrainCount();
}

float MoteFieldAudioProcessor::decibelsToGain (float decibels) noexcept
{
    return std::pow (10.0f, decibels / 20.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout MoteFieldAudioProcessor::createParameterLayout()
{
    using ParameterID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray modeChoices;
    for (const auto* name : motefield::modeNames)
        modeChoices.add (name);

    auto cutoffRange = juce::NormalisableRange<float> (80.0f, 20000.0f);
    cutoffRange.setSkewForCentre (1800.0f);
    auto modulationRateRange = juce::NormalisableRange<float> (0.03f, 8.0f);
    modulationRateRange.setSkewForCentre (0.65f);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { motefield::parameter::mode, 1 }, "Mode", modeChoices, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { motefield::parameter::variation, 1 }, "Variation",
        juce::StringArray { "A", "B", "C", "D" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::density, 1 }, "Activity", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.42f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::repeats, 1 }, "Repeats", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.48f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::shape, 1 }, "Shape", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.52f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { motefield::parameter::division, 1 }, "Subdivision",
        juce::StringArray { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar" }, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::tempo, 1 }, "Tempo",
        juce::NormalisableRange<float> { 40.0f, 240.0f, 0.1f }, 120.0f,
        juce::AudioParameterFloatAttributes().withLabel ("BPM")));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { motefield::parameter::sync, 1 }, "Host Sync", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { motefield::parameter::reverse, 1 }, "Reverse", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { motefield::parameter::freeze, 1 }, "Freeze", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::modDepth, 1 }, "Mod Depth", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.08f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::modRate, 1 }, "Mod Frequency", modulationRateRange, 0.35f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::cutoff, 1 }, "Filter", cutoffRange, 18000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::resonance, 1 }, "Resonance", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.12f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::space, 1 }, "Space", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.28f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { motefield::parameter::reverbStyle, 1 }, "Room",
        juce::StringArray { "Bright", "Dark", "Hall", "Infinite" }, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::mix, 1 }, "Mix", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::output, 1 }, "Output",
        juce::NormalisableRange<float> { -18.0f, 6.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { motefield::parameter::looperLevel, 1 }, "Loop Level", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.75f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { motefield::parameter::looperSpeed, 1 }, "Loop Speed",
        juce::StringArray { "1/2x", "1x", "2x" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { motefield::parameter::looperReverse, 1 }, "Loop Reverse", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { motefield::parameter::looperOrder, 1 }, "Looper Before FX", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { motefield::parameter::bypass, 2 }, "Bypass", false));

    constexpr std::array names { "Loop Record Trigger", "Loop Play Trigger", "Loop Dub Trigger",
                                "Loop Stop Trigger", "Loop Undo Trigger", "Loop Erase Trigger" };
    for (std::size_t i = 0; i < names.size(); ++i)
        layout.add (std::make_unique<juce::AudioParameterBool> (
            ParameterID { motefield::parameter::looperTriggers[i], 3 }, names[i], false));

    addPerformanceParameters (layout);
    // Append parameters; retain the AU ordering/version hints used by old sessions.
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "width", 5 }, "Stereo Width",
        juce::NormalisableRange<float> { 0.f, 2.f, .001f }, 1.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + "%"; })));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "wetSolo", 5 }, "Wet Solo", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "levelMatch", 5 }, "Level Match", false));
    auto referenceRange = juce::NormalisableRange<float> (1.f, 20000.f, .1f);
    referenceRange.setSkewForCentre (440.f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "tuningReference", 6 }, "A4 Reference", referenceRange, 440.f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "loopRecordStart", 6 }, "Recording starts", juce::StringArray { "Follow loop quantize", "Immediately", "Next beat", "Next bar" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "loopCountIn", 6 }, "Recording count-in", juce::StringArray { "Off", "One bar" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "loopLength", 6 }, "Recording length", juce::StringArray { "Free (60 sec max)", "1 bar", "2 bars", "4 bars", "8 bars" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "reverbSolo", 7 }, "Reverb Solo", false));
    return layout;
}

juce::AudioProcessorParameter* MoteFieldAudioProcessor::getBypassParameter() const
{
    return parameters.getParameter (motefield::parameter::bypass);
}

void MoteFieldAudioProcessor::setParameterValue (const char* id, float value)
{
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }
}

juce::StringArray MoteFieldAudioProcessor::factoryPresetNames()
{
    return { "First Light", "Soft Focus", "Slow Motion", "Paper Planes", "Broken Sun", "After Hours", "Tidal", "Blank Canvas",
        "Warm Current", "Petal Drift", "Long Exposure", "Copper Chain", "Skipping Stones", "Soft Landing", "Sideways Rain",
        "Velvet Veil", "Dust Halo", "Near Orbit", "Moon Pool", "Pin Drops", "Glass Seeds", "Pocket Cuts", "Tape Teeth",
        "Fault Lines", "Loose Wires", "Half Steps", "Stairwell", "Clock Garden", "Cross Streets", "Ink Wash", "Night Tide", "Open Water", "Fifth Satellite", "Glass Octave", "Low Tide", "Minor Moon", "Soft Detune" };
}

void MoteFieldAudioProcessor::randomizeSound (juce::int64 seed)
{
    using namespace motefield::parameter;
    juce::Random random(seed);
    const auto range=[&](float low,float high){return juce::jmap(random.nextFloat(),low,high);};
    // Musical starting ranges; performance, output gain and recorded audio are retained.
    setParameterValue(mode,static_cast<float>(random.nextInt(11)));
    setParameterValue(variation,static_cast<float>(random.nextInt(4)));
    setParameterValue(density,range(.22f,.88f));
    setParameterValue(repeats,range(.20f,.76f));
    setParameterValue(shape,range(.08f,.92f));
    setParameterValue(cutoff,std::exp(range(std::log(1800.f),std::log(18000.f))));
    setParameterValue(mix,range(.30f,.72f));
    setParameterValue(space,range(.08f,.55f));
    setParameterValue(modDepth,range(0.f,.30f));
    setParameterValue(modRate,std::exp(range(std::log(.06f),std::log(1.8f))));
    setParameterValue(resonance,range(.02f,.35f));
    setParameterValue(division,static_cast<float>(random.nextInt(9)));
    setParameterValue(reverbStyle,static_cast<float>(random.nextInt(4)));
    setParameterValue(reverse,random.nextBool()?1.f:0.f);
    rememberPreset("Random sound", "generated");
}

void MoteFieldAudioProcessor::applyFactoryPreset (int index)
{
    using namespace motefield::parameter;
    // Original starting points. Performance states and loop routing are preserved.
    struct Preset { int modeIndex, variant; float activity, repeat, contour, filter, wet, reverb, drift; int pulse, room; };
    static constexpr std::array<Preset, 37> presets {{
        {0, 1, .62f, .64f, .40f, 14500.f, .55f, .32f, .08f, 4, 1},
        {3, 2, .72f, .62f, .68f, 7800.f, .64f, .48f, .14f, 4, 2},
        {2, 1, .35f, .78f, .55f, 11200.f, .60f, .38f, .10f, 6, 1},
        {1, 3, .52f, .45f, .34f, 14000.f, .52f, .20f, .04f, 4, 0},
        {6, 1, .74f, .37f, .76f, 16500.f, .60f, .18f, .02f, 2, 1},
        {9, 1, .56f, .66f, .48f, 8200.f, .46f, .28f, .10f, 4, 1},
        {10, 2, .64f, .75f, .72f, 6200.f, .63f, .52f, .22f, 6, 2},
        {0, 0, .42f, .48f, .52f, 18000.f, .50f, .28f, .08f, 4, 1},
        {0, 0, .36f, .48f, .22f, 7200.f, .38f, .24f, .05f, 6, 1},
        {0, 2, .67f, .58f, .68f, 11000.f, .58f, .38f, .12f, 4, 2},
        {0, 3, .31f, .83f, .42f, 9600.f, .61f, .49f, .06f, 8, 2},
        {1, 0, .58f, .55f, .44f, 6400.f, .47f, .22f, .03f, 4, 1},
        {1, 1, .73f, .42f, .78f, 15500.f, .48f, .14f, .04f, 2, 0},
        {2, 0, .29f, .59f, .18f, 10500.f, .44f, .29f, .08f, 6, 1},
        {2, 3, .61f, .68f, .65f, 12400.f, .57f, .33f, .18f, 5, 2},
        {3, 0, .48f, .51f, .25f, 5200.f, .52f, .35f, .07f, 6, 1},
        {3, 3, .84f, .63f, .83f, 13500.f, .62f, .45f, .19f, 3, 2},
        {4, 0, .41f, .46f, .32f, 12000.f, .43f, .23f, .06f, 4, 0},
        {4, 2, .76f, .72f, .59f, 6800.f, .62f, .43f, .21f, 6, 2},
        {5, 0, .32f, .33f, .86f, 16000.f, .44f, .17f, .03f, 2, 0},
        {5, 3, .72f, .52f, .74f, 10200.f, .58f, .39f, .11f, 3, 2},
        {6, 0, .43f, .28f, .67f, 12800.f, .42f, .10f, .02f, 4, 1},
        {6, 3, .85f, .54f, .91f, 7500.f, .57f, .19f, .08f, 0, 0},
        {7, 0, .51f, .35f, .62f, 11200.f, .46f, .16f, .03f, 4, 1},
        {7, 2, .79f, .57f, .79f, 14200.f, .60f, .27f, .14f, 2, 0},
        {8, 1, .44f, .47f, .57f, 12300.f, .45f, .20f, .05f, 4, 1},
        {8, 3, .67f, .68f, .72f, 7900.f, .58f, .41f, .16f, 6, 2},
        {9, 0, .37f, .42f, .52f, 13700.f, .34f, .16f, .02f, 4, 0},
        {9, 3, .74f, .67f, .52f, 9200.f, .48f, .31f, .12f, 3, 1},
        {10, 0, .39f, .57f, .24f, 5600.f, .46f, .36f, .09f, 6, 1},
        {10, 1, .71f, .78f, .65f, 4800.f, .58f, .51f, .23f, 7, 2},
        {10, 3, .86f, .69f, .37f, 12600.f, .57f, .46f, .27f, 5, 0},
        {4, 0, .38f, .38f, .40f, 13200.f, .40f, .18f, .03f, 4, 0},
        {5, 0, .32f, .30f, .75f, 15000.f, .36f, .16f, .02f, 4, 0},
        {10, 0, .35f, .43f, .32f, 6800.f, .38f, .22f, .03f, 6, 1},
        {0, 0, .38f, .38f, .42f, 10800.f, .38f, .20f, .04f, 4, 1},
        {3, 0, .42f, .32f, .35f, 14000.f, .44f, .18f, .02f, 4, 0}
    }};
    index = juce::jlimit (0, static_cast<int> (presets.size()) - 1, index);
    currentProgram.store (index);
    // Every factory patch owns its material, pitch and pattern state. Keep transport and loop configuration.
    for (const auto* id : { "viscosity", "cohesion", "tension", "fieldPosition", "fieldPitch", "fieldStretch", "fieldSplit",
         "magnetAmount", "magnetMode", "magnetAttack", "magnetRelease", "patternSeed", "patternLock", "patternSteps",
         "rhythmMutation", "pitchMutation", "scale", "scaleRoot", "sourceNote", "tuningReference" })
    { auto* p = parameters.getParameter (id); setParameterValue (id, p->convertFrom0to1 (p->getDefaultValue())); }
    if (index >= 32)
    {
        static constexpr std::array<float, 5> transposes { 7.f, 12.f, -12.f, 3.f, .06f };
        setParameterValue ("fieldPitch", transposes[static_cast<std::size_t> (index - 32)]);
    }
    const auto& preset = presets[static_cast<std::size_t> (index)];
    setParameterValue (mode, static_cast<float> (preset.modeIndex));
    setParameterValue (variation, static_cast<float> (preset.variant));
    setParameterValue (density, preset.activity);
    setParameterValue (repeats, preset.repeat);
    setParameterValue (shape, preset.contour);
    setParameterValue (cutoff, preset.filter);
    setParameterValue (mix, preset.wet);
    setParameterValue (space, preset.reverb);
    setParameterValue (modDepth, preset.drift);
    setParameterValue (modRate, .35f);
    setParameterValue (resonance, .12f);
    setParameterValue (division, static_cast<float> (preset.pulse));
    setParameterValue (reverbStyle, static_cast<float> (preset.room));
    setParameterValue (reverse, 0.0f);
    setParameterValue ("width", 1.f);
    rememberPreset (factoryPresetNames()[index], "factory");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoteFieldAudioProcessor();
}
