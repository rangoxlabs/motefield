#pragma once

#include <JuceHeader.h>

#include "DSP.h"
#include <mutex>

namespace motefield::parameter
{
inline constexpr auto mode = "mode";
inline constexpr auto variation = "variation";
inline constexpr auto density = "density";
inline constexpr auto repeats = "repeats";
inline constexpr auto shape = "shape";
inline constexpr auto division = "division";
inline constexpr auto tempo = "tempo";
inline constexpr auto sync = "sync";
inline constexpr auto reverse = "reverse";
inline constexpr auto freeze = "freeze";
inline constexpr auto modDepth = "modDepth";
inline constexpr auto modRate = "modRate";
inline constexpr auto cutoff = "cutoff";
inline constexpr auto resonance = "resonance";
inline constexpr auto space = "space";
inline constexpr auto reverbStyle = "reverbStyle";
inline constexpr auto mix = "mix";
inline constexpr auto output = "output";
inline constexpr auto looperLevel = "looperLevel";
inline constexpr auto looperSpeed = "looperSpeed";
inline constexpr auto looperReverse = "looperReverse";
inline constexpr auto looperOrder = "looperOrder";
inline constexpr auto bypass = "bypass";
inline constexpr std::array<const char*, 6> looperTriggers { "loopRecordTrigger", "loopPlayTrigger", "loopDubTrigger", "loopStopTrigger", "loopUndoTrigger", "loopEraseTrigger" };
} // namespace motefield::parameter

class MoteFieldAudioProcessor final : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Timer
{
public:
    MoteFieldAudioProcessor();
    ~MoteFieldAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 24.0; }

    int getNumPrograms() override { return 37; }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return factoryPresetNames()[juce::jlimit (0,36,index)]; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void requestLooperCommand (motefield::LooperCommand command) noexcept;
    [[nodiscard]] motefield::LooperState getLooperState() const noexcept;
    [[nodiscard]] float getLooperProgress() const noexcept;
    [[nodiscard]] float getInputLevel() const noexcept;
    [[nodiscard]] float getEffectLevel() const noexcept;
    [[nodiscard]] int getActiveGrainCount() const noexcept;
    bool readVisualFrame (motefield::VisualFrame& frame) noexcept { return engine.readVisualFrame (frame); }
    double getEffectiveBpm() const noexcept { return effectiveBpm.load(); }
    bool isReceivingHostTempo() const noexcept { return receivingHostTempo.load(); }
    void setParameterValue (const char* id, float value);
    void randomizeSound (juce::int64 seed);
    void initializeSound();
    void undoInitialization();
    bool canUndoInitialization() const { return initializationUndoAvailable.load(); }
    void applyFactoryPreset (int index);
    static juce::StringArray factoryPresetNames();
    juce::Result exportAudio (const juce::File&, int historyBars = 0);
    juce::Result captureHistory (int bars);
    juce::MemoryBlock loopData();
    bool restoreLoopData (const juce::MemoryBlock&);
    void learnMidi (int parameterIndex) { midiLearn.store (parameterIndex); }
    int learnedController() const { return lastLearned.load(); }
    void clearMidiMappings();
    static const std::vector<juce::String>& extendedParameterIds();

    static juce::File userPresetDirectory();
    static juce::Array<juce::File> userPresetFiles (juce::File directory = {});
    juce::Result saveUserPreset (const juce::String& name, bool overwrite = false, juce::File directory = {});
    juce::Result loadUserPreset (const juce::File&);
    juce::String currentPresetName() const;
    juce::String currentPresetSource() const;
    bool isPresetModified() const;
    static bool isPresetParameter (const juce::String& id);

    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static void addPerformanceParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
    void timerCallback() override;
    void processAudio (juce::AudioBuffer<float>&, int offset, int samples);
    void parameterChanged (const juce::String&, float) override;
    void rememberPreset (const juce::String& name, const juce::String& source);
    static float decibelsToGain (float decibels) noexcept;
    motefield::Engine engine;
    std::array<std::atomic<int>, 128> midiMap;
    std::atomic<int> currentProgram { 0 };
    juce::ValueTree initializationUndo;
    std::atomic<bool> initializationUndoAvailable { false };
    std::atomic<int> midiLearn { -1 }, lastLearned { -1 }, requestedProgram { -1 };
    std::vector<juce::RangedAudioParameter*> midiTargets;
    std::array<int, 128> previousCC {};
    std::atomic<double> beatsPerBar { 4.0 };
    bool previousBurst = false;
    std::mutex archiveMutex;
    juce::MemoryBlock pendingLoopData;
    bool prepared = false;
    double currentSampleRate = 44100.0;

    std::atomic<double> effectiveBpm { 120.0 };
    std::atomic<bool> receivingHostTempo { false };
    std::atomic<unsigned> pendingLooperTriggers { 0 };
    std::array<std::atomic<bool>, 6> previousLooperTriggers {};
    std::atomic<bool> restoringState { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoteFieldAudioProcessor)
};
