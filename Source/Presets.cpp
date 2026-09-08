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
                if (ranged->paramID == "width" || (document->getIntAttribute ("version") == 1 && std::find (extendedParameterIds().begin(), extendedParameterIds().end(), ranged->paramID) != extendedParameterIds().end()))
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
