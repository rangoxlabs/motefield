#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

bool MoteFieldAudioProcessor::isPresetParameter (const juce::String& id)
{
    // A sound preset never starts transport, freezes capture, or bypasses audio.
    if (id == motefield::parameter::freeze || id == motefield::parameter::bypass || id == "burstGate" || id == "wetSolo" || id == "levelMatch") return false;
    for (const auto* trigger : motefield::parameter::looperTriggers) if (id == trigger) return false;
    return true;
}

juce::File MoteFieldAudioProcessor::userPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Rango Labs/MoteField/Presets");
}

juce::Array<juce::File> MoteFieldAudioProcessor::userPresetFiles (juce::File directory)
{
    if (directory == juce::File {}) directory = userPresetDirectory();
    auto files = directory.findChildFiles (juce::File::findFiles, false, "*.motefield");
    std::sort (files.begin(), files.end(), [] (const auto& a, const auto& b)
    { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

juce::String MoteFieldAudioProcessor::currentPresetName() const
{
    return parameters.state.getProperty ("presetName", "Current sound").toString();
}

juce::String MoteFieldAudioProcessor::currentPresetSource() const
{
    return parameters.state.getProperty ("presetSource").toString();
}

void MoteFieldAudioProcessor::rememberPreset (const juce::String& name, const juce::String& source)
{
    initializationUndoAvailable.store (false);
    parameters.state.setProperty ("presetName", name, nullptr);
    parameters.state.setProperty ("presetSource", source, nullptr);
    const auto old = parameters.state.getChildWithName ("PresetBaseline");
    if (old.isValid()) parameters.state.removeChild (old, nullptr);
    juce::ValueTree baseline ("PresetBaseline");
    for (const auto* parameter : getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter))
            if (isPresetParameter (ranged->paramID))
                baseline.setProperty (juce::Identifier (ranged->paramID), ranged->getValue(), nullptr);
    parameters.state.addChild (baseline, -1, nullptr);
}

void MoteFieldAudioProcessor::initializeSound()
{
    using namespace motefield::parameter;
    // Explicit sound controls only: never replace the whole processor state or loop.
    const juce::StringArray controls { variation, density, repeats, shape, cutoff, mix,
        space, modDepth, modRate, resonance, reverbStyle, reverse, "width",
        "viscosity", "cohesion", "tension", "fieldPosition", "fieldPitch", "fieldStretch",
        "fieldSplit", "magnetAmount", "magnetMode", "magnetAttack", "magnetRelease",
        "patternSeed", "patternLock", "patternSteps", "rhythmMutation", "pitchMutation",
        "scale", "scaleRoot", "sourceNote", "tuningReference" };
    const int selectedMode = juce::jlimit (0, 10, juce::roundToInt (parameters.getRawParameterValue (mode)->load()));
    struct StartingSound { float activity, repeat, contour; };
    static constexpr std::array<StartingSound, 11> starts {{
        {.35f, .30f, .40f}, // Bloom: sparse, soft micro loops
        {.40f, .30f, .45f}, // Chain: short linked fragments
        {.30f, .30f, .40f}, // Slide: clear pitch movement
        {.40f, .25f, .30f}, // Veil: a light grain layer
        {.35f, .30f, .40f}, // Orbit: a small moving cluster
        {.30f, .25f, .75f}, // Pluck: defined grain attacks
        {.40f, .20f, .65f}, // Chop: simple rhythmic cuts
        {.35f, .25f, .60f}, // Break: restrained interruptions
        {.35f, .30f, .50f}, // Ladder: clear stepped repeats
        {.35f, .30f, .50f}, // Grid: a short delay pattern
        {.35f, .35f, .30f}  // Smear: soft, short repeats
    }};
    juce::ValueTree previous ("InitializationUndo");
    previous.setProperty ("name", currentPresetName(), nullptr);
    previous.setProperty ("source", currentPresetSource(), nullptr);
    previous.setProperty ("program", currentProgram.load(), nullptr);
    const auto baseline = parameters.state.getChildWithName ("PresetBaseline");
    if (baseline.isValid()) previous.addChild (baseline.createCopy(), -1, nullptr);
    juce::ValueTree values ("Values");
    values.setProperty (mode, parameters.getParameter (mode)->getValue(), nullptr);
    for (const auto& id : controls)
        values.setProperty (juce::Identifier (id), parameters.getParameter (id)->getValue(), nullptr);
    previous.addChild (values, -1, nullptr);

    const auto start = starts[static_cast<std::size_t> (selectedMode)];
    for (const auto& id : controls)
    {
        auto* parameter = parameters.getParameter (id);
        float value = parameter->convertFrom0to1 (parameter->getDefaultValue());
        if (id == density) value = start.activity;
        else if (id == repeats) value = start.repeat;
        else if (id == shape) value = start.contour;
        else if (id == mix) value = .40f;
        else if (id == space || id == modDepth || id == resonance) value = 0.f;
        setParameterValue (id.toRawUTF8(), value);
    }
    rememberPreset (juce::String ("Init - ") + motefield::modeNames[static_cast<std::size_t> (selectedMode)], "initialized");
    initializationUndo = previous;
    initializationUndoAvailable.store (true);
}

void MoteFieldAudioProcessor::undoInitialization()
{
    if (! initializationUndoAvailable.exchange (false)) return;
    const auto values = initializationUndo.getChildWithName ("Values");
    for (int i = 0; i < values.getNumProperties(); ++i)
    {
        const auto id = values.getPropertyName (i);
        auto* parameter = parameters.getParameter (id.toString());
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (static_cast<float> (values[id]));
        parameter->endChangeGesture();
    }
    currentProgram.store (static_cast<int> (initializationUndo["program"]));
    rememberPreset (initializationUndo["name"].toString(), initializationUndo["source"].toString());
    parameters.state.removeChild (parameters.state.getChildWithName ("PresetBaseline"), nullptr);
    const auto baseline = initializationUndo.getChildWithName ("PresetBaseline");
    if (baseline.isValid()) parameters.state.addChild (baseline.createCopy(), -1, nullptr);
    initializationUndo = {};
}

bool MoteFieldAudioProcessor::isPresetModified() const
{
    const auto baseline = parameters.state.getChildWithName ("PresetBaseline");
    if (! baseline.isValid()) return false;
    for (const auto* parameter : getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter))
            if (isPresetParameter (ranged->paramID))
            {
                const auto id = juce::Identifier (ranged->paramID);
                if (! baseline.hasProperty (id) || std::abs (static_cast<float> (baseline[id]) - ranged->getValue()) > .00001f)
                    return true;
            }
    return false;
}

juce::Result MoteFieldAudioProcessor::saveUserPreset (const juce::String& requestedName, bool overwrite, juce::File directory)
{
    const auto name = requestedName.trim();
    const auto stem = name.upToFirstOccurrenceOf (".", false, false).toUpperCase();
    const juce::StringArray reserved { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                     "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    if (name.isEmpty() || name.length() > 80 || name != juce::File::createLegalFileName (name)
        || name.containsAnyOf ("/\\:*?\"<>|") || name.endsWithChar ('.') || reserved.contains (stem))
        return juce::Result::fail ("Use a name of 1-80 characters, without file-path characters or a reserved system name.");
    if (directory == juce::File {}) directory = userPresetDirectory();
    if (const auto result = directory.createDirectory(); result.failed()) return result;
    const auto file = directory.getChildFile (name + ".motefield");
    if (file.exists() && ! overwrite) return juce::Result::fail ("A user preset with this name already exists.");

    juce::XmlElement document ("MoteFieldPreset");
    document.setAttribute ("version", 2);
    document.setAttribute ("name", name);
    juce::ValueTree savedBaseline ("PresetBaseline");
    for (const auto* parameter : getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter))
            if (isPresetParameter (ranged->paramID))
            {
                auto* child = document.createNewChildElement ("PARAM");
                child->setAttribute ("id", ranged->paramID);
                const auto normalized = ranged->getValue();
                child->setAttribute ("value", static_cast<double> (normalized));
                savedBaseline.setProperty (juce::Identifier (ranged->paramID), normalized, nullptr);
            }
    const auto audioData = loopData();
    if (audioData.getSize() > 17) document.createNewChildElement ("LoopAudio")->addTextElement (audioData.toBase64Encoding());
    // Write beside the destination and replace only after a complete, flushed write.
    juce::TemporaryFile temporary (file);
    if (! document.writeTo (temporary.getFile()) || ! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("The preset could not be written. Your existing preset has not been intentionally overwritten.");
    rememberPreset (name, "user");
    parameters.state.removeChild (parameters.state.getChildWithName ("PresetBaseline"), nullptr);
    parameters.state.addChild (savedBaseline, -1, nullptr);
    return juce::Result::ok();
}

juce::Result MoteFieldAudioProcessor::loadUserPreset (const juce::File& file)
{
    if (! file.existsAsFile() || file.getSize() > 256 * 1024 * 1024)
        return juce::Result::fail ("This preset is missing or too large.");
    const auto document = juce::XmlDocument::parse (file);
    if (document == nullptr || ! document->hasTagName ("MoteFieldPreset") || (document->getIntAttribute ("version", -1) != 1 && document->getIntAttribute ("version", -1) != 2))
        return juce::Result::fail ("This is not a supported MoteField preset.");
    std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
    std::set<juce::String> seen;
    for (const auto* child : document->getChildIterator())
    {
        if (child->hasTagName ("LoopAudio")) continue;
        const auto id = child->getStringAttribute ("id");
        auto* parameter = parameters.getParameter (id);
        const auto text = child->getStringAttribute ("value").trim();
        char* end = nullptr;
        const auto number = std::strtod (text.toRawUTF8(), &end);
        if (! child->hasTagName ("PARAM") || parameter == nullptr || ! isPresetParameter (id)
            || ! seen.insert (id).second || text.isEmpty() || end == nullptr || *end != '\0'
            || ! std::isfinite (number) || number < 0.0 || number > 1.0)
            return juce::Result::fail ("This preset contains invalid parameter data. Your sound has not changed.");
        values.emplace_back (parameter, static_cast<float> (number));
    }
    for (const auto* parameter : getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter))
            if (isPresetParameter (ranged->paramID) && seen.count (ranged->paramID) == 0)
            {
                if ((ranged->paramID == "width" || ranged->paramID == "tuningReference" || ranged->paramID == "loopRecordStart" || ranged->paramID == "loopCountIn" || ranged->paramID == "loopLength") || (document->getIntAttribute ("version") == 1 && std::find (extendedParameterIds().begin(), extendedParameterIds().end(), ranged->paramID) != extendedParameterIds().end()))
                    values.emplace_back (parameters.getParameter (ranged->paramID), ranged->getDefaultValue());
                else return juce::Result::fail ("This preset is incomplete. Your sound has not changed.");
            }
    if (const auto* audio = document->getChildByName ("LoopAudio"))
    { juce::MemoryBlock bytes; if (! bytes.fromBase64Encoding (audio->getAllSubText()) || ! restoreLoopData (bytes))
        return juce::Result::fail ("The saved loop could not be loaded. Resume audio playback and try again."); }
    // Validation finishes before any host-visible values change.
    for (const auto& [parameter, normalized] : values)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalized);
        parameter->endChangeGesture();
    }
    rememberPreset (file.getFileNameWithoutExtension(), "user");
    return juce::Result::ok();
}
