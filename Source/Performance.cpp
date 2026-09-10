#include "PluginProcessor.h"
#include <cmath>

namespace
{
struct Control { const char* id; const char* name; float low, high, initial, step; const char* choices; };
const Control controls[] {
    {"loopQuantize", "Quantize loop", 0,1,0,1,"Off|Beat"},
    {"loopContinuous", "Continuous loop speed",0,1,0,1,"Stepped|Continuous"},
    {"loopRate", "Loop rate",.25f,4,1,.001f,nullptr},
    {"loopFade", "Loop fade seconds",0,10,0,.01f,nullptr},
    {"loopFadeMode", "Loop fade direction",0,2,0,1,"In and out|In only|Out only"},
    {"loopOnly", "Looper only",0,1,0,1,"Off|On"},
    {"loopRecordOrder", "Close recording into",0,1,0,1,"Playback|Overdub"},
    {"burstGate", "Burst record gate",0,1,0,1,"Released|Recording"},
    {"bypassTrails", "Bypass trails",0,1,0,1,"Off|On"},
    {"holdStyle", "Hold behavior",0,1,0,1,"Toggle|Momentary"},
    {"viscosity", "Viscosity",0,1,.25f,.001f,nullptr},
    {"cohesion", "Cohesion",0,1,.5f,.001f,nullptr},
    {"tension", "Tension",0,1,.5f,.001f,nullptr},
    {"fieldPosition", "Capture position",0,1,0,.001f,nullptr},
    {"fieldPitch", "Field pitch semitones",-24,24,0,.01f,nullptr},
    {"fieldStretch", "Grain stretch",.25f,4,1,.001f,nullptr},
    {"fieldSplit", "Split voices",0,1,0,.001f,nullptr},
    {"magnetAmount", "Sidechain magnet",0,1,0,.001f,nullptr},
    {"magnetMode", "Magnet action",0,2,0,1,"Compress|Trigger|Attract"},
    {"magnetAttack", "Magnet attack seconds",.001f,.5f,.01f,.001f,nullptr},
    {"magnetRelease", "Magnet release seconds",.01f,3,.25f,.01f,nullptr},
    {"patternSeed", "Pattern seed",1,65535,1,1,nullptr},
    {"patternLock", "Lock pattern",0,1,0,1,"Evolving|Locked"},
    {"patternSteps", "Pattern length",1,64,16,1,nullptr},
    {"rhythmMutation", "Rhythm mutation",0,127,0,1,nullptr},
    {"pitchMutation", "Pitch mutation",0,127,0,1,nullptr},
    {"scale", "Pitch scale",0,4,0,1,"Free|Major|Minor|Major pentatonic|Minor pentatonic"},
    {"scaleRoot", "Scale root",0,11,0,1,"C|C#|D|D#|E|F|F#|G|G#|A|A#|B"},
    {"sourceNote", "Source note",0,11,0,1,"C|C#|D|D#|E|F|F#|G|G#|A|A#|B"}
};
juce::MemoryBlock encode (const motefield::AudioSnapshot& snapshot)
{
    juce::MemoryBlock data;
    juce::MemoryOutputStream output (data, false);
    output.writeInt (0x4d464c33); output.writeDouble (snapshot.sampleRate); output.writeBool (snapshot.playing);
    output.writeInt (static_cast<int> (snapshot.audio[0].size()));
    for (const auto& c : snapshot.audio) for (float sample : c) output.writeFloat (sample);
    return data;
}
bool decode (const juce::MemoryBlock& bytes, motefield::AudioSnapshot& snapshot)
{
    if (bytes.getSize() < 17) return false;
    juce::MemoryInputStream input (bytes, false);
    if (input.readInt() != 0x4d464c33) return false;
    snapshot.sampleRate = input.readDouble(); snapshot.playing = input.readBool();
    const auto count = input.readInt();
    if (! std::isfinite (snapshot.sampleRate) || snapshot.sampleRate < 8000 || snapshot.sampleRate > 384000
        || count < 0 || count > snapshot.sampleRate * 60.0 || bytes.getSize() != 17u + static_cast<std::size_t> (count) * 8u) return false;
    for (auto& c : snapshot.audio) { c.resize (static_cast<std::size_t> (count)); for (auto& sample : c) { sample = input.readFloat(); if (! std::isfinite (sample)) return false; } }
    return true;
}
}

const std::vector<juce::String>& MoteFieldAudioProcessor::extendedParameterIds()
{
    static const auto ids = [] { std::vector<juce::String> result; for (const auto& c : controls) result.emplace_back (c.id); for (const auto* id : { "width", "wetSolo", "levelMatch", "tuningReference", "loopRecordStart", "loopCountIn", "loopLength" }) result.emplace_back (id); return result; }();
    return ids;
}
void MoteFieldAudioProcessor::addPerformanceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    for (const auto& c : controls)
    {
        if (c.choices) layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { c.id, 4 }, c.name,
            juce::StringArray::fromTokens (c.choices, "|", ""), static_cast<int> (c.initial)));
        else layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { c.id, 4 }, c.name,
            juce::NormalisableRange<float> (c.low, c.high, c.step), c.initial));
    }
}
juce::MemoryBlock MoteFieldAudioProcessor::loopData()
{
    std::lock_guard<std::mutex> lock (archiveMutex);
    return prepared ? encode (engine.snapshotLoop()) : pendingLoopData.getSize() > 0 ? pendingLoopData : encode (motefield::AudioSnapshot {});
}
bool MoteFieldAudioProcessor::restoreLoopData (const juce::MemoryBlock& bytes)
{
    motefield::AudioSnapshot snapshot;
    if (! decode (bytes, snapshot)) return false;
    std::lock_guard<std::mutex> lock (archiveMutex);
    if (prepared && ! engine.restoreLoop (snapshot)) return false;
    pendingLoopData = bytes;
    return true;
}
juce::Result MoteFieldAudioProcessor::exportAudio (const juce::File& file, int historyBars)
{
    motefield::AudioSnapshot snapshot;
    { std::lock_guard<std::mutex> lock (archiveMutex);
      if (! prepared) return juce::Result::fail ("Start audio playback before exporting.");
      snapshot = historyBars > 0 ? engine.snapshotHistory (historyBars * beatsPerBar.load() * 60.0 / getEffectiveBpm()) : engine.snapshotLoop(); }
    if (snapshot.audio[0].empty()) return juce::Result::fail ("There is no recorded audio to export yet.");
    juce::TemporaryFile temp (file);
    auto stream = temp.getFile().createOutputStream();
    if (! stream) return juce::Result::fail ("Cannot write to this location.");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.get(), snapshot.sampleRate, 2, 24, {}, 0));
    if (! writer) return juce::Result::fail ("Cannot create the WAV file.");
    stream.release();
    const float* audio[] { snapshot.audio[0].data(), snapshot.audio[1].data() };
    if (! writer->writeFromFloatArrays (audio, 2, static_cast<int> (snapshot.audio[0].size()))) return juce::Result::fail ("The audio file could not be completed.");
    writer.reset();
    return temp.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail ("Could not finish saving the audio file.");
}
juce::Result MoteFieldAudioProcessor::captureHistory (int bars)
{
    std::lock_guard<std::mutex> lock (archiveMutex);
    if (! prepared) return juce::Result::fail ("Start audio playback first.");
    const auto snapshot = engine.snapshotHistory (juce::jlimit (1,4,bars) * beatsPerBar.load() * 60.0 / getEffectiveBpm());
    if (snapshot.audio[0].empty()) return juce::Result::fail ("Play some audio before capturing history.");
    return engine.restoreLoop (snapshot) ? juce::Result::ok() : juce::Result::fail ("The previous capture is still loading. Try again after playback resumes.");
}
void MoteFieldAudioProcessor::setCurrentProgram (int index) { requestedProgram.store (juce::jlimit (0,36,index)); }
void MoteFieldAudioProcessor::timerCallback()
{
    const auto index = requestedProgram.exchange (-1);
    if (index >= 0) applyFactoryPreset (index);
}
void MoteFieldAudioProcessor::clearMidiMappings()
{
    for (auto& mapping : midiMap) mapping.store (-1);
    lastLearned.store (-1); midiLearn.store (-1);
}
