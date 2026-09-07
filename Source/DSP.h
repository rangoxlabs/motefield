#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace motefield
{
enum class Mode : int
{
    bloom = 0,
    chain,
    slide,
    veil,
    orbit,
    pluck,
    chop,
    breakUp,
    ladder,
    grid,
    smear,
    count
};

inline constexpr std::array<const char*, static_cast<std::size_t> (Mode::count)> modeNames {
    "Bloom", "Chain", "Slide", "Veil", "Orbit", "Pluck",
    "Chop", "Break", "Ladder", "Grid", "Smear"
};

enum class LooperCommand : int
{
    none = 0,
    recordPlayDub,
    stopPlay,
    undo,
    clear,
    record,
    play,
    dub,
    stop, burstStart, burstEnd
};

enum class LooperState : int
{
    empty = 0,
    recording,
    playing,
    overdubbing,
    stopped
};

struct EngineParameters
{
    Mode mode = Mode::bloom;
    int variation = 0;
    float density = 0.42f;
    float repeats = 0.48f;
    float shape = 0.52f;
    int division = 4;
    double bpm = 120.0;
    bool reverse = false;
    bool freeze = false;
    float modulationDepth = 0.08f;
    float modulationRateHz = 0.35f;
    float cutoffHz = 18000.0f;
    float resonance = 0.12f;
    float space = 0.28f;
    int reverbStyle = 1;
    float mix = 0.5f;
    float outputGain = 1.0f;
    float looperLevel = 0.75f;
    float looperSpeed = 1.0f;
    bool looperReverse = false;
    bool looperBeforeEffect = false;
    bool bypass = false;
    bool quantize = false, looperOnly = false, trails = false;
    float loopFade = 0.0f;
    int fadeMode = 0;
    bool recordIntoDub = false;
    double hostPpq = 0.0;
    bool hostPositionValid = false, hostPlaying = false;
    float viscosity = 0.25f, cohesion = 0.5f, tension = 0.5f;
    float fieldPosition = 0.0f, fieldPitch = 0.0f, fieldStretch = 1.0f, fieldSplit = 0.0f;
    float magnetAmount = 0.0f, magnetAttack = 0.01f, magnetRelease = 0.25f;
    int magnetMode = 0;
    const float* sidechain = nullptr;
    int seed = 1, rhythmMutation = 0, pitchMutation = 0, scale = 0, root = 0, sourceNote = 0;
    bool patternLock = false;
    int patternSteps = 16;

};

// A bounded, audio-thread-produced snapshot. The editor never reads live DSP buffers.
inline float shapeEnvelopePower (float shape) noexcept
{
    return 0.45f + (shape < 0.0f ? 0.0f : shape > 1.0f ? 1.0f : shape) * 2.35f;
}

struct VisualVoice
{
    std::uint64_t id = 0;
    float phase = 0.0f, duration = 0.0f, rate = 1.0f, pan = 0.0f;
    float envelope = 0.0f, level = 0.0f;
    float envelopePower = 1.0f;
    bool delayTap = false;
    std::array<float, 24> waveform {};
};

struct VisualFrame
{
    static constexpr std::size_t outputPoints = 192;
    std::uint64_t sampleTime = 0;
    Mode mode = Mode::bloom;
    int variation = 0, voiceCount = 0;
    double bpm = 120.0;
    float inputLevel = 0.0f, effectLevel = 0.0f, outputLevel = 0.0f;
    // Post-output low/mid/high band RMS envelopes; visual analysis only.
    std::array<float, 3> spectralEnergy {};
    float viscosity = .25f, cohesion = .5f, tension = .5f, magnet = 0.f;
    float fieldPosition = 0.f, fieldPitch = 0.f, fieldStretch = 1.f, fieldSplit = 0.f;
    bool loopPending = false;
    float loopSeconds = 0.0f, loopProgress = 0.0f;
    LooperState loopState = LooperState::empty;
    bool held = false, reverse = false, bypass = false, canUndo = false;
    std::array<VisualVoice, 54> voices {};
    std::array<float, 128> loopWaveform {};
    // Chronological min/max bins of the final stereo output (~320 ms).
    std::array<std::array<float, outputPoints>, 2> outputLow {}, outputHigh {};
};

struct AudioSnapshot
{
    double sampleRate = 44100.0;
    bool playing = false;
    std::array<std::vector<float>, 2> audio;
};

class Engine
{
public:
    Engine();
    ~Engine();

    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    void prepare (double sampleRate, int maximumBlockSize, int channels = 2);
    void reset();

    void process (const float* const* inputs,
                  float* const* outputs,
                  int channels,
                  int samples,
                  const EngineParameters& parameters);

    // File conversion and snapshot allocation happen on the caller, never in process().
    AudioSnapshot snapshotLoop();
    bool restoreLoop (const AudioSnapshot&);
    AudioSnapshot snapshotHistory (double seconds);
    void requestLooperCommand (LooperCommand command) noexcept;
    [[nodiscard]] LooperState getLooperState() const noexcept;
    [[nodiscard]] float getLooperProgress() const noexcept;
    [[nodiscard]] float getInputLevel() const noexcept;
    [[nodiscard]] float getEffectLevel() const noexcept;
    [[nodiscard]] int getActiveGrainCount() const noexcept;
    // One consumer (the editor). Drains stale frames without blocking the audio thread.
    bool readVisualFrame (VisualFrame& destination) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace motefield
