#include "DSP.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>
#include <mutex>

namespace motefield
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

template <typename T>
T clamp (T value, T low, T high)
{
    return std::max (low, std::min (value, high));
}

float lerp (float a, float b, float t)
{
    return a + (b - a) * t;
}

float saturate (float x)
{
    return std::tanh (x);
}

double wrapPosition (double position, double length)
{
    while (position < 0.0)
        position += length;
    while (position >= length)
        position -= length;
    return position;
}

float divisionFactor (int division)
{
    constexpr std::array<float, 9> factors {
        0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.5f,
        2.0f / 3.0f, 1.0f, 2.0f, 4.0f
    };
    return factors[static_cast<std::size_t> (clamp (division, 0, 8))];
}

class Random
{
public:
    void reset() noexcept { state = 0x7139a52du; }
    void seed (std::uint32_t value) noexcept { state = value == 0 ? 1 : value; }

    std::uint32_t nextU32() noexcept
    {
        auto x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    float nextFloat() noexcept
    {
        return static_cast<float> (nextU32() & 0x00ffffffu) / 16777216.0f;
    }

    float bipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

private:
    std::uint32_t state = 0x7139a52du;
};

class StereoRing
{
public:
    void prepare (int requestedSize)
    {
        size = std::max (requestedSize, 64);
        for (auto& channel : data)
            channel.assign (static_cast<std::size_t> (size), 0.0f);
        clear();
    }

    void clear()
    {
        for (auto& channel : data)
            std::fill (channel.begin(), channel.end(), 0.0f);
        writeIndex = 0;
        filled = 0;
    }

    void write (float left, float right) noexcept
    {
        data[0][static_cast<std::size_t> (writeIndex)] = left;
        data[1][static_cast<std::size_t> (writeIndex)] = right;
        writeIndex = (writeIndex + 1) % size;
        filled = std::min (filled + 1, size);
    }

    [[nodiscard]] float read (int channel, double delaySamples) const noexcept
    {
        if (size < 4 || filled < 3)
            return 0.0f;

        const auto maximumDelay = static_cast<double> (std::min (filled - 2, size - 2));
        if (delaySamples < 0.0 || delaySamples > maximumDelay)
            return 0.0f;

        auto position = static_cast<double> (writeIndex - 1) - delaySamples;
        position = wrapPosition (position, static_cast<double> (size));
        const auto index0 = static_cast<int> (position);
        const auto index1 = (index0 + 1) % size;
        const auto fraction = static_cast<float> (position - static_cast<double> (index0));
        const auto& buffer = data[static_cast<std::size_t> (clamp (channel, 0, 1))];
        return lerp (buffer[static_cast<std::size_t> (index0)],
                     buffer[static_cast<std::size_t> (index1)], fraction);
    }

    [[nodiscard]] int available() const noexcept { return filled; }
    [[nodiscard]] int capacity() const noexcept { return size; }

private:
    std::array<std::vector<float>, 2> data;
    int size = 0;
    int writeIndex = 0;
    int filled = 0;
};

struct Grain
{
    std::uint64_t id = 0;
    float level = 0.0f;
    bool active = false;
    double delay = 0.0;
    double rate = 1.0;
    double rateDelta = 0.0;
    float age = 0.0f;
    float duration = 1.0f;
    float gain = 1.0f;
    float pan = 0.0f;
    float envelopePower = 1.0f;
    float readSpan = 96000.f, tone = 1.f, filterStateL = 0.f, filterStateR = 0.f;

    void start (double newDelay,
                double startRate,
                double endRate,
                float newDuration,
                float newGain,
                float newPan,
                float contour) noexcept
    {
        active = true;
        delay = newDelay;
        rate = startRate;
        duration = std::max (newDuration, 8.0f);
        rateDelta = (endRate - startRate) / static_cast<double> (duration);
        gain = newGain;
        pan = clamp (newPan, -1.0f, 1.0f);
        envelopePower = shapeEnvelopePower (contour);
        age = 0.0f;
        level = 0.0f; filterStateL = filterStateR = 0.f;
    }

    void process (const StereoRing& ring,
                  bool captureHeadIsMoving,
                  const EngineParameters& material,
                  float& left,
                  float& right) noexcept
    {
        if (! active)
            return;

        const auto phase = age / duration;
        if (phase >= 1.0f || delay < 0.0 || delay >= static_cast<double> (ring.available() - 2))
        {
            active = false;
            return;
        }

        const auto hann = std::sin (pi * clamp (phase, 0.0f, 1.0f));
        const auto envelope = std::pow (std::max (hann, 0.0f), clamp (envelopePower + (material.tension - .5f) * 3.f, .2f, 6.f)) * gain;
        const auto panAngle = (clamp (pan * (1.5f - material.cohesion), -1.f, 1.f) + 1.0f) * pi * 0.25f;
        const auto gainL = std::cos (panAngle) * 1.41421356f;
        const auto gainR = std::sin (panAngle) * 1.41421356f;
        const auto offset = material.fieldPosition * std::min (static_cast<float> (ring.available()) * .35f, readSpan);
        filterStateL += tone * (ring.read (0, delay + offset) - filterStateL);
        filterStateR += tone * (ring.read (1, delay + offset) - filterStateR);
        const auto sampleL = filterStateL * envelope * gainL;
        const auto sampleR = filterStateR * envelope * gainR;
        left += sampleL;
        right += sampleR;
        level = std::max ({ std::abs (sampleL), std::abs (sampleR), level * 0.9995f });

        age += 1.0f / clamp (material.fieldStretch, .25f, 4.f);
        rate += rateDelta / clamp (material.fieldStretch, .25f, 4.f);
        delay += (captureHeadIsMoving ? 1.0 : 0.0) - rate * std::pow (2.0, material.fieldPitch / 12.0);
    }
};

class OnsetDetector
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        fastRelease = std::exp (-1.0f / static_cast<float> (0.025 * sampleRate));
        slowCoefficient = std::exp (-1.0f / static_cast<float> (0.16 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        fast = 0.0f;
        slow = 0.0f;
        refractory = 0;
    }

    bool process (float left, float right) noexcept
    {
        const auto level = std::max (std::abs (left), std::abs (right));
        fast = level > fast ? level : fastRelease * fast + (1.0f - fastRelease) * level;
        slow = slowCoefficient * slow + (1.0f - slowCoefficient) * level;

        if (refractory > 0)
            --refractory;

        if (refractory == 0 && fast > slow * 1.85f + 0.018f && level > 0.025f)
        {
            refractory = static_cast<int> (sampleRate * 0.055);
            return true;
        }
        return false;
    }

private:
    double sampleRate = 44100.0;
    float fast = 0.0f;
    float slow = 0.0f;
    float fastRelease = 0.99f;
    float slowCoefficient = 0.999f;
    int refractory = 0;
};

class StateVariableLowPass
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    void set (float cutoffHz, float resonance) noexcept
    {
        const auto cutoff = clamp (cutoffHz, 20.0f, static_cast<float> (sampleRate * 0.475));
        g = std::tan (pi * cutoff / static_cast<float> (sampleRate));
        k = 2.0f - 1.92f * clamp (resonance, 0.0f, 1.0f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float input) noexcept
    {
        const auto v3 = input - ic2eq;
        const auto v1 = a1 * ic1eq + a2 * v3;
        const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return v2;
    }

private:
    double sampleRate = 44100.0;
    float g = 0.1f;
    float k = 2.0f;
    float a1 = 1.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
};

class MultiTapDelay
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        ring.prepare (static_cast<int> (sampleRate * 12.0));
        holdHistory.prepare (static_cast<int> (sampleRate * 8.0));
        reset();
    }

    void reset()
    {
        ring.clear();
        holdHistory.clear();
        wasHeld = false;
        holdPosition = 0.0;
        sampleClock = 0;
        tapLevels.fill (0.0f); pitchPhases.fill (0.0); tapLow = {}; tapUpper = {};
        heldLevel = 0.0f;
        feedbackL = feedbackR = 0.0f;
        smearStateL = smearStateR = 0.0f;
        lfoPhase = 0.0;
    }

    void process (float inputL,
                  float inputR,
                  const EngineParameters& parameters,
                  float& outputL,
                  float& outputR) noexcept
    {
        const auto quarter = sampleRate * 60.0 / clamp (parameters.bpm, 30.0, 300.0);
        const auto baseDelay = clamp (quarter * divisionFactor (parameters.division) * parameters.fieldStretch,
                                      24.0,
                                      static_cast<double> (ring.capacity() - 64));
        const auto variation = clamp (parameters.variation, 0, 3);
        const auto tapCount = clamp (1 + static_cast<int> (parameters.density * 5.99f + parameters.fieldSplit * 4.f), 1, 6);

        if (parameters.freeze)
        {
            if (! wasHeld)
            {
                holdLength = std::min (baseDelay * 4.0, static_cast<double> (holdHistory.available() - 4));
                holdPosition = 0.0;
            }
            wasHeld = true;
            outputL = outputR = 0.0f;
            if (holdLength > 8.0)
            {
                // Replay the captured delay phrase with an overlapped seam; no buffer copy.
                const auto seam = std::min (sampleRate * 0.008, holdLength * 0.1);
                const auto fade = static_cast<float> (clamp ((holdPosition - (holdLength - seam)) / seam, 0.0, 1.0));
                const auto second = holdPosition - (holdLength - seam);
                outputL = lerp (holdHistory.read (0, holdLength - holdPosition),
                                holdHistory.read (0, holdLength - std::max (0.0, second)), fade);
                outputR = lerp (holdHistory.read (1, holdLength - holdPosition),
                                holdHistory.read (1, holdLength - std::max (0.0, second)), fade);
                holdPosition += 1.0;
                if (holdPosition >= holdLength) holdPosition = seam;
            }
            heldLevel = std::max ({ std::abs (outputL), std::abs (outputR), heldLevel * 0.9995f });
            ++sampleClock;
            return;
        }
        wasHeld = false;

        static constexpr std::array<std::array<float, 6>, 4> patterns {{
            {{ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f }},
            {{ 0.5f, 1.0f, 1.5f, 2.5f, 3.0f, 4.0f }},
            {{ 0.75f, 1.0f, 1.25f, 2.0f, 2.75f, 3.5f }},
            {{ 0.3333f, 1.0f, 1.6667f, 2.0f, 2.6667f, 4.0f }}
        }};

        outputL = 0.0f;
        outputR = 0.0f;
        auto weightSum = 0.0f;
        const auto isSmear = parameters.mode == Mode::smear;
        const auto modulation = isSmear ? std::sin (static_cast<float> (lfoPhase))
                                              * (0.01f + 0.055f * parameters.shape)
                                        : 0.0f;

        for (int tap = 0; tap < tapCount; ++tap)
        {
            const auto pan = tapCount == 1 ? 0.0f
                                           : ((static_cast<float> (tap) / static_cast<float> (tapCount - 1)) * 2.0f - 1.0f) * (1.5f - parameters.cohesion);
            const auto delay = baseDelay * patterns[static_cast<std::size_t> (variation)][static_cast<std::size_t> (tap)]
                               * (1.0 + modulation * (tap + 1)) * (parameters.rhythmMutation != 0 ? .5 + .25 * ((tap + parameters.rhythmMutation) % 7) : 1.0);
            const auto weight = 1.0f / std::sqrt (static_cast<float> (tap + 1));
            const auto ti = static_cast<std::size_t> (tap);
            const auto semitones = parameters.fieldPitch + (parameters.pitchMutation != 0 ? static_cast<float> (((parameters.pitchMutation * 3 + tap * 2) % 5) * 3 - 6) : 0.f) + (isSmear && variation == 2 ? 12.f : 0.f);
            const auto window = sampleRate * .08;
            auto& pitchPhase = pitchPhases[ti];
            pitchPhase = wrapPosition (pitchPhase + (1.0 - std::pow (2.0, semitones / 12.0)) / window, 1.0);
            const auto readTap = [&] (int channel, double tapDelay)
            {
                tapDelay += parameters.fieldPosition * sampleRate * 2.0;
                if (std::abs (semitones) < .001f) return ring.read (channel, tapDelay);
                const auto second = std::fmod (pitchPhase + .5, 1.0);
                const auto weight = static_cast<float> (std::pow (std::sin (pi * pitchPhase), 2.0));
                return ring.read (channel, tapDelay + pitchPhase * window) * weight + ring.read (channel, tapDelay + second * window) * (1.f - weight);
            };
            auto sampleL = readTap (0, delay), sampleR = readTap (1, delay * (1.0 + .003 * pan));
            if (isSmear || std::abs (parameters.tension - .5f) > .001f)
            {
                const auto sweep = .5f + .5f * std::sin (static_cast<float> (lfoPhase) * (tap + 1) + tap);
                const auto coefficient = clamp (.03f + (1.f - parameters.shape) * .25f + sweep * .15f + (.5f - parameters.tension) * .2f, .005f, .9f);
                for (int c = 0; c < 2; ++c)
                {
                    auto& signal = c == 0 ? sampleL : sampleR;
                    tapLow[c][ti] += coefficient * (signal - tapLow[c][ti]);
                    tapUpper[c][ti] += coefficient * .2f * (signal - tapUpper[c][ti]);
                    signal = isSmear && variation == 1 ? (tapLow[c][ti] - tapUpper[c][ti]) * 1.4f : tapLow[c][ti];
                }
            }
            tapDelays[static_cast<std::size_t> (tap)] = delay;
            tapLevels[static_cast<std::size_t> (tap)] = std::max ({ std::abs (sampleL * weight), std::abs (sampleR * weight),
                tapLevels[static_cast<std::size_t> (tap)] * 0.9995f });
            outputL += (sampleL * (1.0f - 0.25f * pan) + sampleR * 0.12f * (1.0f + pan)) * weight;
            outputR += (sampleR * (1.0f + 0.25f * pan) + sampleL * 0.12f * (1.0f - pan)) * weight;
            weightSum += weight;
        }

        if (weightSum > 0.0f)
        {
            outputL /= weightSum;
            outputR /= weightSum;
        }

        if (isSmear)
        {
            const auto coefficient = 0.035f + 0.35f * (1.0f - parameters.shape);
            smearStateL += coefficient * (outputL - smearStateL);
            smearStateR += coefficient * (outputR - smearStateR);
            if (variation == 0 || variation == 1)
            {
                outputL = smearStateL;
                outputR = smearStateR;
            }
            else if (variation == 2)
            {
                outputL = 0.65f * outputL + 0.55f * ring.read (0, baseDelay * 0.5);
                outputR = 0.65f * outputR + 0.55f * ring.read (1, baseDelay * 0.505);
            }
            else
            {
                outputL = 0.55f * outputL + 0.5f * ring.read (0, baseDelay * 1.5);
                outputR = 0.55f * outputR + 0.5f * ring.read (1, baseDelay * 1.493);
            }
        }

        const auto feedback = 0.08f + parameters.repeats * 0.78f;
        const auto cross = parameters.mode == Mode::smear ? 0.22f + 0.2f * parameters.shape : 0.08f;
        ring.write (saturate (inputL + feedback * (feedbackL * (1.0f - cross) + feedbackR * cross)),
                    saturate (inputR + feedback * (feedbackR * (1.0f - cross) + feedbackL * cross)));
        feedbackL = outputL;
        feedbackR = outputR;
        holdHistory.write (outputL, outputR);
        ++sampleClock;

        lfoPhase += 2.0 * static_cast<double> (pi) * (0.035 + parameters.modulationRateHz * 0.08) / sampleRate;
        if (lfoPhase >= 2.0 * static_cast<double> (pi))
            lfoPhase -= 2.0 * static_cast<double> (pi);
    }

    void appendVisuals (VisualFrame& frame, float activity) const noexcept
    {
        const auto count = wasHeld ? 1 : clamp (1 + static_cast<int> (activity * 5.99f), 1, 6);
        for (int tap = 0; tap < count; ++tap)
        {
            const auto index = static_cast<std::size_t> (tap);
            const auto level = wasHeld ? heldLevel : tapLevels[index];
            if (level < 0.00005f) continue;
            auto& voice = frame.voices[static_cast<std::size_t> (frame.voiceCount++)];
            voice.id = 1000000u + static_cast<std::uint64_t> (tap);
            voice.delayTap = true;
            voice.level = level;
            const auto duration = wasHeld ? std::max (holdLength, 1.0) : std::max (tapDelays[index], 1.0);
            voice.duration = static_cast<float> (duration / sampleRate);
            voice.phase = static_cast<float> (wasHeld ? holdPosition / duration : std::fmod (static_cast<double> (sampleClock), duration) / duration);
            voice.pan = count == 1 ? 0.0f : static_cast<float> (tap) / static_cast<float> (count - 1) * 2.0f - 1.0f;
            voice.rate = 1.0f;
            voice.envelope = 1.0f;
            for (std::size_t point = 0; point < voice.waveform.size(); ++point)
            {
                const auto offset = static_cast<double> (point) * sampleRate * 0.035 / static_cast<double> (voice.waveform.size());
                voice.waveform[point] = wasHeld ? holdHistory.read (0, wrapPosition (holdLength - holdPosition + offset, std::max (holdLength, 1.0)))
                                               : ring.read (0, tapDelays[index] + offset);
            }
        }
    }

private:
    StereoRing ring;
    StereoRing holdHistory;
    std::array<double, 6> tapDelays {};
    std::array<float, 6> tapLevels {};
    std::uint64_t sampleClock = 0;
    double holdLength = 0.0, holdPosition = 0.0;
    float heldLevel = 0.0f;
    bool wasHeld = false;
    std::array<double, 6> pitchPhases {};
    std::array<std::array<float, 6>, 2> tapLow {}, tapUpper {};
    double sampleRate = 44100.0;
    double lfoPhase = 0.0;
    float feedbackL = 0.0f;
    float feedbackR = 0.0f;
    float smearStateL = 0.0f;
    float smearStateR = 0.0f;
};

class ModulatedDelay
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        ring.prepare (static_cast<int> (sampleRate * 0.09));
        reset();
    }

    void reset()
    {
        ring.clear();
        phase = 0.0;
    }

    void process (float& left, float& right, float depth, float rateHz) noexcept
    {
        depth = clamp (depth, 0.0f, 1.0f);
        ring.write (left, right);
        if (depth < 0.0001f)
            return;

        const auto sweepMs = 1.5 + 10.0 * depth;
        const auto centreMs = 4.0 + 13.0 * depth;
        const auto delayL = (centreMs + sweepMs * std::sin (phase)) * sampleRate * 0.001;
        const auto delayR = (centreMs + sweepMs * std::sin (phase + 1.57079632679)) * sampleRate * 0.001;
        const auto delayedL = ring.read (0, delayL);
        const auto delayedR = ring.read (1, delayR);
        const auto blend = 0.56f * depth;
        left = left * (1.0f - blend * 0.35f) + delayedL * blend;
        right = right * (1.0f - blend * 0.35f) + delayedR * blend;

        phase += 2.0 * static_cast<double> (pi) * clamp (static_cast<double> (rateHz), 0.02, 12.0) / sampleRate;
        if (phase >= 2.0 * static_cast<double> (pi))
            phase -= 2.0 * static_cast<double> (pi);
    }

private:
    StereoRing ring;
    double sampleRate = 44100.0;
    double phase = 0.0;
};

class ReverseCloud
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        ring.prepare (static_cast<int> (sampleRate * 8.0));
        reset();
    }

    void reset()
    {
        ring.clear();
        phase = 0.0;
    }

    void process (float& left, float& right, bool enabled, double bpm, int division) noexcept
    {
        ring.write (left, right);
        const auto requestedLength = sampleRate * 60.0 / clamp (bpm, 30.0, 300.0) * divisionFactor (division);
        const auto length = clamp (requestedLength, 256.0, sampleRate * 2.5);
        phase = wrapPosition (phase, length);

        if (enabled && ring.available() > static_cast<int> (length * 2.0 + 4.0))
        {
            const auto phaseB = wrapPosition (phase + length * 0.5, length);
            const auto windowA = std::pow (std::sin (pi * static_cast<float> (phase / length)), 2.0f);
            const auto windowB = std::pow (std::sin (pi * static_cast<float> (phaseB / length)), 2.0f);
            const auto normalizer = 1.0f / std::max (windowA + windowB, 0.001f);
            left = (ring.read (0, 2.0 * phase + 2.0) * windowA
                    + ring.read (0, 2.0 * phaseB + 2.0) * windowB) * normalizer;
            right = (ring.read (1, 2.0 * phase + 2.0) * windowA
                     + ring.read (1, 2.0 * phaseB + 2.0) * windowB) * normalizer;
        }

        phase += 1.0;
    }

private:
    StereoRing ring;
    double sampleRate = 44100.0;
    double phase = 0.0;
};

class CombFilter
{
public:
    void prepare (int length)
    {
        buffer.assign (static_cast<std::size_t> (std::max (length, 8)), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        index = 0;
        filterStore = 0.0f;
    }

    float process (float input, float feedback, float damping) noexcept
    {
        const auto output = buffer[static_cast<std::size_t> (index)];
        filterStore = output * (1.0f - damping) + filterStore * damping;
        buffer[static_cast<std::size_t> (index)] = input + filterStore * feedback;
        index = (index + 1) % static_cast<int> (buffer.size());
        return output;
    }

private:
    std::vector<float> buffer;
    int index = 0;
    float filterStore = 0.0f;
};

class AllPassFilter
{
public:
    void prepare (int length)
    {
        buffer.assign (static_cast<std::size_t> (std::max (length, 8)), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        index = 0;
    }

    float process (float input) noexcept
    {
        const auto delayed = buffer[static_cast<std::size_t> (index)];
        const auto output = delayed - input;
        buffer[static_cast<std::size_t> (index)] = input + delayed * 0.52f;
        index = (index + 1) % static_cast<int> (buffer.size());
        return output;
    }

private:
    std::vector<float> buffer;
    int index = 0;
};

class StereoReverb
{
public:
    void prepare (double sampleRate)
    {
        const auto scale = sampleRate / 44100.0;
        constexpr std::array<int, 6> combLengths { 1116, 1188, 1277, 1356, 1422, 1491 };
        constexpr std::array<int, 3> allPassLengths { 225, 341, 556 };

        for (std::size_t i = 0; i < combL.size(); ++i)
        {
            combL[i].prepare (static_cast<int> (combLengths[i] * scale));
            combR[i].prepare (static_cast<int> ((combLengths[i] + 23) * scale));
        }
        for (std::size_t i = 0; i < allPassL.size(); ++i)
        {
            allPassL[i].prepare (static_cast<int> (allPassLengths[i] * scale));
            allPassR[i].prepare (static_cast<int> ((allPassLengths[i] + 17) * scale));
        }
        reset();
    }

    void reset()
    {
        for (auto& filter : combL) filter.reset();
        for (auto& filter : combR) filter.reset();
        for (auto& filter : allPassL) filter.reset();
        for (auto& filter : allPassR) filter.reset();
    }

    void process (float inputL,
                  float inputR,
                  float amount,
                  int style,
                  float& outputL,
                  float& outputR) noexcept
    {
        static constexpr std::array<float, 4> feedbacks { 0.68f, 0.76f, 0.84f, 0.91f };
        static constexpr std::array<float, 4> dampings { 0.14f, 0.52f, 0.28f, 0.38f };
        static constexpr std::array<float, 4> widths { 0.38f, 0.52f, 0.68f, 0.82f };
        style = clamp (style, 0, 3);
        const auto feedback = feedbacks[static_cast<std::size_t> (style)] * (0.82f + amount * 0.16f);
        const auto damping = dampings[static_cast<std::size_t> (style)];
        const auto width = widths[static_cast<std::size_t> (style)];
        const auto mono = (inputL + inputR) * (0.075f + amount * 0.025f);

        auto sumL = 0.0f;
        auto sumR = 0.0f;
        for (std::size_t i = 0; i < combL.size(); ++i)
        {
            sumL += combL[i].process (mono + inputL * 0.025f, feedback, damping);
            sumR += combR[i].process (mono + inputR * 0.025f, feedback, damping);
        }
        sumL *= 0.19f;
        sumR *= 0.19f;
        for (auto& filter : allPassL) sumL = filter.process (sumL);
        for (auto& filter : allPassR) sumR = filter.process (sumR);

        outputL = sumL * (1.0f - width * 0.3f) + sumR * width * 0.3f;
        outputR = sumR * (1.0f - width * 0.3f) + sumL * width * 0.3f;
    }

private:
    std::array<CombFilter, 6> combL;
    std::array<CombFilter, 6> combR;
    std::array<AllPassFilter, 3> allPassL;
    std::array<AllPassFilter, 3> allPassR;
};

class PhraseLooper
{
    struct Storage
    {
        explicit Storage (int n) : capacity (n)
        {
            for (auto& c : base) c = std::make_unique<std::atomic<float>[]> (static_cast<std::size_t> (n));
            for (auto& c : overdub) c = std::make_unique<std::atomic<float>[]> (static_cast<std::size_t> (n));
            stamps = std::make_unique<std::atomic<std::uint64_t>[]> (static_cast<std::size_t> (n));
            for (int i = 0; i < n; ++i) { stamps[i] = 0; for (int c = 0; c < 2; ++c) { base[c][i] = 0; overdub[c][i] = 0; } }
            for (auto& flag : layers) flag.store (false);
            layers[1] = true;
        }
        float read (int c, int i) const noexcept
        {
            for (;;)
            {
                const auto revision = stamps[i].load (std::memory_order_acquire);
                if ((revision & 0x10000u) != 0) continue;
                const auto stamp = static_cast<unsigned> (revision & 65535u);
                const auto value = base[c][i].load (std::memory_order_relaxed)
                    + (stamp != 0 && layers[stamp].load (std::memory_order_relaxed) ? overdub[c][i].load (std::memory_order_relaxed) : 0.f);
                if (stamps[i].load (std::memory_order_acquire) == revision) return value;
            }
        }
        std::array<std::unique_ptr<std::atomic<float>[]>, 2> base, overdub;
        std::unique_ptr<std::atomic<std::uint64_t>[]> stamps;
        std::array<std::atomic<bool>, 65536> layers;
        int capacity;
        std::atomic<int> length { 0 };
        bool resume = false;
    };
public:
    void prepare (double rate)
    {
        std::lock_guard<std::mutex> lock (archiveMutex);
        sampleRate = rate;
        slots[0] = std::make_unique<Storage> (static_cast<int> (std::ceil (rate * 60.0)));
        slots[1].reset(); slots[2].reset(); active.store (0); pendingSlot.store (-1); reader.store (-1);
        reset();
    }
    void reset() noexcept
    {
        if (slots[active.load()]) slots[active.load()]->length.store (0);
        position = 0; recordPosition = 0; generation = 1; canUndo = false;
        state.store (static_cast<int> (LooperState::empty)); progress.store (0);
        commandRead.store (0); commandWrite.store (0); pendingCommand = LooperCommand::none;
        pending.store (false); internalBeat = 0; fadeGain = 0; lastBeat = -1; speedSmooth = 1;
    }
    void beginBlock() noexcept
    {
        const auto next = pendingSlot.load (std::memory_order_acquire);
        if (next >= 0)
        {
            active.store (next, std::memory_order_release);
            position = 0; recordPosition = 0; generation = 1; canUndo = false; fadeGain = 0;
            const auto& storage = *slots[next];
            state.store (static_cast<int> (storage.length.load() > 1 ? (storage.resume ? LooperState::playing : LooperState::stopped) : LooperState::empty));
            pendingCommand = LooperCommand::none; pending.store (false); progress.store (0);
            pendingSlot.store (-1, std::memory_order_release);
        }
    }
    AudioSnapshot snapshot()
    {
        std::lock_guard<std::mutex> lock (archiveMutex);
        int slot;
        do { slot = pendingSlot.load (std::memory_order_acquire); if (slot < 0) slot = active.load (std::memory_order_acquire); reader.store (slot, std::memory_order_release); }
        while (slot != active.load (std::memory_order_acquire) && slot != pendingSlot.load (std::memory_order_acquire));
        AudioSnapshot result; result.sampleRate = sampleRate;
        const auto* data = slots[slot].get();
        if (data != nullptr)
        {
            const auto length = data->length.load (std::memory_order_acquire);
            for (auto& c : result.audio) c.resize (static_cast<std::size_t> (length));
            for (int i = 0; i < length; ++i) for (int c = 0; c < 2; ++c) result.audio[c][i] = data->read (c, i);
            const auto st = currentState(); result.playing = slot == pendingSlot.load() ? data->resume : st == LooperState::playing || st == LooperState::overdubbing || st == LooperState::recording;
        }
        reader.store (-1, std::memory_order_release); return result;
    }
    bool restore (const AudioSnapshot& audio)
    {
        std::lock_guard<std::mutex> lock (archiveMutex);
        if (pendingSlot.load (std::memory_order_acquire) >= 0 || audio.sampleRate <= 0 || audio.audio[0].size() != audio.audio[1].size()) return false;
        int target = 0;
        while (target == active.load (std::memory_order_acquire) || target == reader.load()) ++target;
        if (target >= 3) return false;
        auto storage = std::make_unique<Storage> (static_cast<int> (std::ceil (sampleRate * 60.0)));
        const auto count = std::min (storage->capacity, static_cast<int> (audio.audio[0].size() * sampleRate / audio.sampleRate));
        for (int i = 0; i < count; ++i)
        {
            const auto source = i * audio.sampleRate / sampleRate;
            const auto a = std::min (static_cast<std::size_t> (source), audio.audio[0].size() - 1);
            const auto b = std::min (a + 1, audio.audio[0].size() - 1);
            for (int c = 0; c < 2; ++c) storage->base[c][i].store (lerp (audio.audio[c][a], audio.audio[c][b], static_cast<float> (source - a)));
        }
        storage->length.store (count); storage->resume = audio.playing;
        slots[target] = std::move (storage);
        pendingSlot.store (target, std::memory_order_release); return true;
    }
    void request (LooperCommand command) noexcept
    {
        const auto write = commandWrite.load (std::memory_order_relaxed), next = (write + 1u) % commands.size();
        if (next == commandRead.load (std::memory_order_acquire)) return;
        commands[write] = command; commandWrite.store (next, std::memory_order_release);
    }
    LooperState currentState() const noexcept { return static_cast<LooperState> (state.load()); }
    float currentProgress() const noexcept { return progress.load(); }
    void copyVisual (VisualFrame& frame) const noexcept
    {
        const auto& data = *slots[active.load()]; const auto length = data.length.load();
        frame.loopState = currentState(); frame.loopSeconds = static_cast<float> (length / sampleRate);
        frame.loopProgress = currentProgress(); frame.canUndo = canUndo; frame.loopPending = pending.load();
        if (length < 2) return;
        for (std::size_t bin = 0; bin < frame.loopWaveform.size(); ++bin)
            for (int point = 0; point < 16; ++point)
            {
                const auto i = std::min (length - 1, static_cast<int> ((bin + point / 16.0) * length / frame.loopWaveform.size()));
                frame.loopWaveform[bin] = std::max ({ frame.loopWaveform[bin], std::abs (data.read (0, i)), std::abs (data.read (1, i)) });
            }
    }
    void process (float liveL, float liveR, const EngineParameters& p, int sample, float& outL, float& outR) noexcept
    {
        auto& data = *slots[active.load (std::memory_order_relaxed)];
        const auto quarter = sampleRate * 60.0 / clamp (p.bpm, 30.0, 300.0);
        const auto beat = p.hostPositionValid && p.hostPlaying ? p.hostPpq + sample / quarter : internalBeat;
        internalBeat += 1.0 / quarter;
        auto read = commandRead.load (std::memory_order_relaxed);
        if (read != commandWrite.load (std::memory_order_acquire))
        {
            const auto cmd = commands[read]; commandRead.store ((read + 1) % commands.size(), std::memory_order_release);
            const auto immediate = cmd == LooperCommand::clear || cmd == LooperCommand::undo || cmd == LooperCommand::burstStart || cmd == LooperCommand::burstEnd;
            if (p.quantize && ! immediate) { pendingCommand = cmd; targetBeat = std::ceil (beat - 1.e-7); pending.store (true); }
            else { if (cmd != LooperCommand::undo) pending.store (false); execute (cmd, p, beat); }
        }
        // Rebase an armed command after a host seek so it never waits for an obsolete timeline position.
        if (lastBeat >= 0 && std::abs (beat - lastBeat - 1.0 / quarter) > .05 && pending.load()) targetBeat = std::ceil (beat - 1.e-7);
        if (pending.load() && beat + 1.e-7 >= targetBeat) { execute (pendingCommand, p, beat); pending.store (false); }
        auto st = currentState();
        outL = liveL; outR = liveR;
        if (st == LooperState::recording)
        {
            if (recordPosition < data.capacity)
            {
                const auto tag = data.stamps[recordPosition].load(); data.stamps[recordPosition].store (tag + 0x10000u);
                data.base[0][recordPosition].store (liveL); data.base[1][recordPosition].store (liveR);
                data.stamps[recordPosition].store ((tag + 0x20000u) & ~std::uint64_t (65535), std::memory_order_release);
                ++recordPosition; data.length.store (recordPosition, std::memory_order_release);
                progress.store (static_cast<float> (recordPosition) / data.capacity);
            }
            if (recordPosition >= data.capacity) finish (false, p);
        }
        else if ((st == LooperState::playing || st == LooperState::overdubbing || stopping) && data.length.load() > 1)
        {
            const auto length = data.length.load();
            if (p.quantize && p.hostPositionValid && p.hostPlaying && lastBeat >= 0 && std::abs (beat - lastBeat - 1.0 / quarter) > .05)
                position = wrapPosition ((beat - originBeat) * quarter * p.looperSpeed, length);
            const bool fadeIn = p.fadeMode != 2, fadeOut = p.fadeMode != 1;
            const auto fadeStep = p.loopFade > .001f ? 1.f / (p.loopFade * static_cast<float> (sampleRate)) : 1.f;
            fadeGain = stopping ? std::max (0.f, fadeGain - (fadeOut ? fadeStep : 1.f)) : std::min (1.f, fadeGain + (fadeIn ? fadeStep : 1.f));
            const auto a = static_cast<int> (position), b = (a + 1) % length; const auto t = static_cast<float> (position - a);
            const auto edge = 1.f;
            outL += lerp (data.read (0, a), data.read (0, b), t) * p.looperLevel * fadeGain * edge;
            outR += lerp (data.read (1, a), data.read (1, b), t) * p.looperLevel * fadeGain * edge;
            if (st == LooperState::overdubbing)
            {
                const auto i = clamp (static_cast<int> (std::llround (position)), 0, length - 1);
                const auto tag = data.stamps[i].load(); const auto stamp = static_cast<unsigned> (tag & 65535u);
                data.stamps[i].store (tag + 0x10000u);
                if (stamp != generation)
                {
                    if (stamp != 0 && data.layers[stamp].load()) for (int c = 0; c < 2; ++c) data.base[c][i].store (data.base[c][i].load() + data.overdub[c][i].load());
                    data.overdub[0][i].store (liveL); data.overdub[1][i].store (liveR);
                }
                else { data.overdub[0][i].store (saturate (data.overdub[0][i].load() + liveL)); data.overdub[1][i].store (saturate (data.overdub[1][i].load() + liveR)); }
                data.stamps[i].store (((tag + 0x20000u) & ~std::uint64_t (65535)) | generation, std::memory_order_release);
                canUndo = true;
            }
            speedSmooth += (1.f - std::exp (-1.f / static_cast<float> (sampleRate * .01))) * (p.looperSpeed - speedSmooth);
            position = wrapPosition (position + (p.looperReverse ? -1 : 1) * speedSmooth, length);
            progress.store (static_cast<float> (position / length));
            if (stopping && fadeGain <= 0) { stopping = false; state.store (static_cast<int> (LooperState::stopped)); }
        }
        lastBeat = beat;
    }
private:
    void finish (bool stop, const EngineParameters& p) noexcept
    {
        auto& data = *slots[active.load()];
        for (int i = recordPosition; i < 2; ++i) { const auto tag = data.stamps[i].load(); data.stamps[i].store (tag + 0x10000u); data.base[0][i].store (0); data.base[1][i].store (0); data.stamps[i].store ((tag + 0x20000u) & ~std::uint64_t (65535)); }
        data.length.store (std::max (recordPosition, 2));
        position = 0; fadeGain = 0; progress.store (0);
        state.store (static_cast<int> (stop ? LooperState::stopped : p.recordIntoDub ? LooperState::overdubbing : LooperState::playing));
    }
    void execute (LooperCommand cmd, const EngineParameters& p, double beat) noexcept
    {
        auto& data = *slots[active.load()]; auto st = currentState();
        if (cmd == LooperCommand::clear) { data.length.store (0); state.store (static_cast<int> (LooperState::empty)); canUndo = false; stopping = false; pending.store (false); progress.store (0); return; }
        if (cmd == LooperCommand::undo) { if (canUndo) { data.layers[generation].store (false); canUndo = false; if (st == LooperState::overdubbing) state.store (static_cast<int> (LooperState::playing)); } return; }
        if (cmd == LooperCommand::burstStart) { data.length.store (0); st = LooperState::empty; cmd = LooperCommand::record; }
        if (cmd == LooperCommand::burstEnd) { if (st == LooperState::recording) { auto options = p; options.recordIntoDub = false; finish (false, options); } return; }
        if (cmd == LooperCommand::stop || (cmd == LooperCommand::stopPlay && st != LooperState::stopped))
        { if (st == LooperState::recording) finish (true, p); else if (st == LooperState::playing || st == LooperState::overdubbing) { stopping = p.loopFade > 0 && p.fadeMode != 1; state.store (static_cast<int> (stopping ? LooperState::playing : LooperState::stopped)); } return; }
        if (cmd == LooperCommand::record && st != LooperState::empty && st != LooperState::recording) return;
        if (cmd == LooperCommand::play || (cmd == LooperCommand::stopPlay && st == LooperState::stopped))
        { if (st == LooperState::recording) finish (false, p); else if (st != LooperState::empty) { state.store (static_cast<int> (LooperState::playing)); fadeGain = 0; stopping = false; } return; }
        if (cmd == LooperCommand::dub && st != LooperState::playing && st != LooperState::overdubbing) return;
        if (st == LooperState::empty) { recordPosition = 0; generation = 1; data.layers[1].store (true); canUndo = false; originBeat = beat; stopping = false; state.store (static_cast<int> (LooperState::recording)); }
        else if (st == LooperState::recording) finish (false, p);
        else if (st == LooperState::playing) { if (generation < 65535) { ++generation; data.layers[generation].store (true); canUndo = false; state.store (static_cast<int> (LooperState::overdubbing)); } }
        else { stopping = false; state.store (static_cast<int> (LooperState::playing)); }
    }
    std::array<std::unique_ptr<Storage>, 3> slots;
    std::atomic<int> active { 0 }, pendingSlot { -1 }, reader { -1 };
    std::mutex archiveMutex;
    std::array<LooperCommand, 64> commands {};
    std::atomic<std::size_t> commandRead { 0 }, commandWrite { 0 };
    std::atomic<int> state { 0 }; std::atomic<float> progress { 0 }; std::atomic<bool> pending { false };
    LooperCommand pendingCommand = LooperCommand::none;
    double position = 0, sampleRate = 44100, internalBeat = 0, originBeat = 0, lastBeat = -1, targetBeat = 0;
    int recordPosition = 0; unsigned generation = 1;
    bool canUndo = false, stopping = false;
    float fadeGain = 0, speedSmooth = 1;
};
} // namespace

struct Engine::Impl
{
    static constexpr int maximumGrains = 48;

    void prepare (double newSampleRate, int newMaximumBlockSize, int)
    {
        sampleRate = clamp (newSampleRate, 8000.0, 384000.0);
        maximumBlockSize = std::max (newMaximumBlockSize, 16);
        capture.prepare (static_cast<int> (sampleRate * 12.0));
        historySize = static_cast<int> (sampleRate * 32.0);
        for (auto& c : history) c = std::make_unique<std::atomic<float>[]> (static_cast<std::size_t> (historySize));
        historyClock.store (0);
        delay.prepare (sampleRate);
        modulation.prepare (sampleRate);
        reverseCloud.prepare (sampleRate);
        onset.prepare (sampleRate);
        filterL.prepare (sampleRate);
        filterR.prepare (sampleRate);
        reverb.prepare (sampleRate);
        looper.prepare (sampleRate);
        resizeScratch (maximumBlockSize);
        reset();
    }

    void reset()
    {
        capture.clear(); historyClock.store (0);
        fieldPitch = fieldPosition = magnetEnvelope = 0.f; fieldStretch = 1.f;
        wasTrailsBypassed = false; wasHostPlaying = false; expectedPpq = 0; previousSeed = -1;
        delay.reset();
        modulation.reset();
        reverseCloud.reset();
        onset.reset();
        filterL.reset();
        filterR.reset();
        reverb.reset();
        looper.reset();
        for (auto& grain : grains) grain.active = false;
        onsetAges.fill (-1.0);
        onsetCount = 0;
        spawnCountdown = 0.0;
        sequenceStep = 0;
        feedbackL = feedbackR = 0.0f;
        smoothedMix = 0.5f;
        smoothedCutoff = 18000.0f;
        smoothedOutput = 1.0f;
        smoothedBypass = 0.0f;
        sampleClock = 0;
        visualCountdown = 0;
        voiceSequence = 0;
        outputPeak = 0.0f;
        visualLow = visualUpper = {};visualBandPower = {};
        visualLowCoefficient = 1.f - std::exp (-2.f * pi * 180.f / static_cast<float> (sampleRate));
        visualUpperCoefficient = 1.f - std::exp (-2.f * pi * 2500.f / static_cast<float> (sampleRate));
        visualAttack = 1.f - std::exp (-1.f / (.006f * static_cast<float> (sampleRate)));
        visualRelease = 1.f - std::exp (-1.f / (.075f * static_cast<float> (sampleRate)));
        outputLow = {};
        outputHigh = {};
        scopeLow = scopeHigh = {};
        scopeWrite = 0;
        scopeSamples = 0;
        scopeBinSamples = std::max (1, static_cast<int> (sampleRate / 600.0));
        random.reset();
        inputLevel.store (0.0f);
        effectLevel.store (0.0f);
        activeGrains.store (0);
    }

    void resizeScratch (int samples)
    {
        for (auto& channel : dry) channel.resize (static_cast<std::size_t> (samples));
        for (auto& channel : stage) channel.resize (static_cast<std::size_t> (samples));
        for (auto& channel : wet) channel.resize (static_cast<std::size_t> (samples));
    }

    void rememberOnset() noexcept
    {
        for (int i = static_cast<int> (onsetAges.size()) - 1; i > 0; --i)
            onsetAges[static_cast<std::size_t> (i)] = onsetAges[static_cast<std::size_t> (i - 1)];
        onsetAges[0] = 18.0;
        onsetCount = std::min (onsetCount + 1, static_cast<int> (onsetAges.size()));
    }

    Grain* findFreeGrain() noexcept
    {
        for (auto& grain : grains)
            if (! grain.active)
                return &grain;

        auto* oldest = &grains[0];
        for (auto& grain : grains)
            if (grain.age / grain.duration > oldest->age / oldest->duration)
                oldest = &grain;
        return oldest;
    }

    double recentOnsetDelay (int offset) const noexcept
    {
        if (onsetCount <= 0)
            return -1.0;
        const auto index = std::abs (offset) % onsetCount;
        return onsetAges[static_cast<std::size_t> (index)];
    }

    double chooseReadDelay (Mode mode, double grid, int voice) noexcept
    {
        const auto available = static_cast<double> (capture.available() - 4);
        if (available < 64.0)
            return -1.0;

        if ((mode == Mode::pluck || mode == Mode::ladder) && onsetCount > 0)
        {
            const auto remembered = recentOnsetDelay (eventStep + voice);
            if (remembered > 0.0 && remembered < available)
                return remembered;
        }

        const auto history = std::min (available, std::max (grid * 4.0, sampleRate * 0.12));
        if (mode == Mode::veil)
            return 24.0 + random.nextFloat() * std::min (history, sampleRate * 2.5);
        if (mode == Mode::orbit)
            return 32.0 + random.nextFloat() * std::min (history, sampleRate * 1.2);
        if (mode == Mode::chain || mode == Mode::chop || mode == Mode::breakUp)
        {
            const auto slices = 1 + (eventStep + voice * 3) % 8;
            return clamp (grid * 0.25 * static_cast<double> (slices), 20.0, history);
        }
        return 24.0 + random.nextFloat() * std::min (history, sampleRate * 0.8);
    }

    std::pair<double, double> ratesFor (Mode mode, int variation, int voice) noexcept
    {
        variation = clamp (variation, 0, 3);
        const auto index = static_cast<std::size_t> ((eventStep + voice) & 3);
        static constexpr std::array<double, 4> upDown { 0.5, 1.0, 2.0, 4.0 };
        static constexpr std::array<double, 4> ladderA { 1.0, 1.259921, 1.498307, 2.0 };
        static constexpr std::array<double, 4> ladderB { 1.0, 0.749154, 0.5, 1.33484 };

        switch (mode)
        {
            case Mode::bloom:
                if (variation == 0) return { voice % 2 == 0 ? 1.0 : 2.0, voice % 2 == 0 ? 1.0 : 2.0 };
                if (variation == 1) return { voice % 2 == 0 ? 1.0 : 0.5, voice % 2 == 0 ? 1.0 : 0.5 };
                if (variation == 2) return { 2.0, 2.0 };
                return { upDown[index], upDown[index] };
            case Mode::chain:
                if (variation == 1) return { index % 2 == 0 ? 1.0 : 0.5, index % 2 == 0 ? 1.0 : 0.5 };
                if (variation == 3) return { upDown[index] * 0.5, upDown[index] * 0.5 };
                return { 1.0, 1.0 };
            case Mode::slide:
                if (variation == 0) return { 0.5, 1.0 };
                if (variation == 1) return { 2.0, 0.5 };
                if (variation == 2) return { 1.0, 2.0 };
                return voice % 2 == 0 ? std::pair<double, double> { 0.5, 2.0 }
                                      : std::pair<double, double> { 2.0, 0.5 };
            case Mode::veil:
                if (variation == 0) return { 0.94 + pitchRandom.nextFloat() * 0.12, 0.94 + pitchRandom.nextFloat() * 0.12 };
                if (variation == 1) return { 0.72 + pitchRandom.nextFloat() * 0.62, 0.72 + pitchRandom.nextFloat() * 0.62 };
                if (variation == 2) return { voice % 2 == 0 ? 1.0 : 2.0, voice % 2 == 0 ? 1.0 : 2.0 };
                return { voice % 2 == 0 ? 1.0 : 0.5, voice % 2 == 0 ? 1.0 : 0.5 };
            case Mode::orbit:
                if (variation == 1) return { 0.5, 0.5 };
                if (variation == 3) return { 0.72, 1.35 };
                return { 1.0, 1.0 };
            case Mode::pluck:
                if (variation == 1) return { 0.985 + 0.01 * voice, 0.985 + 0.01 * voice };
                if (variation == 3) return { voice % 3 == 0 ? 2.0 : 1.0, voice % 3 == 0 ? 2.0 : 1.0 };
                return { 1.0, 1.0 };
            case Mode::chop:
            case Mode::breakUp:
                if (variation == 1) return { upDown[index], upDown[index] };
                if (variation == 3) return { upDown[(index + 1) & 3] * 0.5, upDown[(index + 1) & 3] * 0.5 };
                return { 0.88 + pitchRandom.nextFloat() * 0.24, 0.88 + pitchRandom.nextFloat() * 0.24 };
            case Mode::ladder:
                if (variation == 0) return { ladderA[index], ladderA[index] };
                if (variation == 1) return { ladderB[index], ladderB[index] };
                if (variation == 2) return { upDown[index], upDown[index] };
                return { ladderA[(index * 3) & 3], ladderA[(index * 3) & 3] };
            case Mode::grid:
            case Mode::smear:
            case Mode::count:
            default:
                return { 1.0, 1.0 };
        }
    }

    static std::uint32_t hash (std::uint32_t a) noexcept
    { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; return a ^ (a >> 16); }
    float rhythmValue (const EngineParameters& p) const noexcept
    { return static_cast<float> (hash (static_cast<unsigned> (p.seed + p.rhythmMutation * 7919) ^ static_cast<unsigned> (sequenceStep % std::max (1, p.patternSteps))) & 65535u) / 65535.f; }
    double tunedRate (double rate, const EngineParameters& p) const noexcept
    {
        if (p.scale == 0) return rate;
        constexpr unsigned masks[] { 4095, 2741, 1453, 661, 1193 };
        const auto note = 12.0 * std::log2 (std::max (.01, rate)) + p.sourceNote;
        int best = static_cast<int> (std::round (note)); double distance = 100;
        for (int i = static_cast<int> (std::floor (note)) - 12; i <= static_cast<int> (std::ceil (note)) + 12; ++i)
            if ((masks[clamp (p.scale, 0, 4)] & (1u << ((i - p.root + 120) % 12))))
                if (std::abs (i - note) < distance) { best = i; distance = std::abs (i - note); }
        return std::pow (2.0, (best - p.sourceNote) / 12.0);
    }
    void spawnEvent (const EngineParameters& parameters, double grid, bool onsetHit) noexcept
    {
        const auto step = parameters.patternLock ? sequenceStep % std::max (1, parameters.patternSteps) : sequenceStep;
        eventStep = step;
        random.seed (hash (static_cast<unsigned> (parameters.seed) ^ static_cast<unsigned> (step + 1)));
        const auto mode = parameters.mode;
        if (mode == Mode::grid)
            return;
        if (mode == Mode::breakUp && ! onsetHit && rhythmValue (parameters) > 0.2f + parameters.density * 0.62f)
        { ++sequenceStep; return; }
        if (mode == Mode::pluck && onsetCount == 0)
            return;

        auto voices = 1 + static_cast<int> (parameters.density * 3.99f);
        if (mode == Mode::veil) voices = 2 + static_cast<int> (parameters.density * 5.99f);
        if (mode == Mode::orbit) voices = 1 + static_cast<int> (parameters.density * 2.99f);
        if (mode == Mode::chop || mode == Mode::breakUp) voices = 1 + static_cast<int> (parameters.density * 2.99f);
        if (mode == Mode::ladder) voices = 1;
        if (mode == Mode::smear) voices = 1 + static_cast<int> (parameters.density * 1.99f);

        voices += static_cast<int> (parameters.fieldSplit * 4.f);
        for (int voice = 0; voice < voices; ++voice)
        {
            const auto readDelay = chooseReadDelay (mode, grid, voice);
            if (readDelay < 0.0)
                continue;

            pitchRandom.seed (hash (static_cast<unsigned> (parameters.seed + parameters.pitchMutation * 104729) ^ static_cast<unsigned> (step * 17 + voice + 1)));
            auto rates = ratesFor (mode, parameters.variation, voice);
            if (parameters.pitchMutation != 0)
            {
                constexpr std::array<double,6> intervals { 1.,1.189207,1.33484,1.498307,2.,.5 };
                const auto interval = intervals[pitchRandom.nextU32() % intervals.size()]; rates.first *= interval; rates.second *= interval;
            }
            rates.first = tunedRate (rates.first, parameters); rates.second = tunedRate (rates.second, parameters);
            if (parameters.reverse)
            {
                rates.first = -rates.first;
                rates.second = -rates.second;
            }

            float duration = static_cast<float> (grid * (0.32 + parameters.repeats * 1.35));
            if (mode == Mode::bloom) duration = static_cast<float> (grid * (0.55 + parameters.repeats * 2.4));
            if (mode == Mode::chain) duration = static_cast<float> (grid * (0.45 + parameters.repeats * 1.1));
            if (mode == Mode::slide) duration = static_cast<float> (grid * (0.75 + parameters.repeats * 1.8));
            if (mode == Mode::veil) duration = static_cast<float> (sampleRate * (0.055 + parameters.shape * 0.38 + parameters.repeats * 0.12));
            if (mode == Mode::orbit) duration = static_cast<float> (grid * (2.0 + parameters.repeats * 7.0));
            if (mode == Mode::pluck) duration = static_cast<float> (grid * (0.25 + parameters.repeats * 1.6));
            if (mode == Mode::chop || mode == Mode::breakUp) duration = static_cast<float> (grid * (0.18 + parameters.repeats * 0.72));
            if (mode == Mode::ladder) duration = static_cast<float> (grid * (0.38 + parameters.repeats * 0.52));
            if (mode == Mode::smear) duration = static_cast<float> (grid * (0.6 + parameters.repeats * 1.8));

            duration = clamp (duration, 24.0f, static_cast<float> (sampleRate * 4.0));
            const auto gain = (mode == Mode::veil ? 0.42f : 0.66f) / std::sqrt (static_cast<float> (voices));
            const auto pan = voices == 1 ? random.bipolar() * 0.25f
                                         : -0.8f + 1.6f * static_cast<float> (voice) / static_cast<float> (voices - 1);
            auto* grain = findFreeGrain();
            grain->id = ++voiceSequence;
            grain->start (readDelay + voice * 7.0,
                                    rates.first,
                                    rates.second,
                                    duration,
                                    gain,
                                    pan + random.bipolar() * 0.12f,
                                    parameters.shape);
            grain->readSpan = static_cast<float> (sampleRate * 2.0);
            grain->tone = 1.f;
            if ((mode == Mode::chain && parameters.variation == 0) || (mode == Mode::ladder && parameters.variation == 2))
                grain->tone = .02f + random.nextFloat() * .45f;
            if ((mode == Mode::orbit && parameters.variation == 2) || (mode == Mode::chop && parameters.variation == 2))
                grain->tone = .03f + parameters.shape * .2f;
        }
        ++sequenceStep;
    }

    double intervalFor (const EngineParameters& parameters, double grid) const noexcept
    {
        switch (parameters.mode)
        {
            case Mode::bloom: return grid / (1.0 + parameters.density * 2.6);
            case Mode::chain: return grid;
            case Mode::slide: return grid * (0.65 - parameters.density * 0.25);
            case Mode::veil: return sampleRate * (0.115 - parameters.density * 0.095);
            case Mode::orbit: return grid * (1.5 - parameters.density * 0.8);
            case Mode::pluck: return grid * (1.0 - parameters.density * 0.55);
            case Mode::chop: return grid * (0.75 - parameters.density * 0.4);
            case Mode::breakUp: return grid * (1.4 - parameters.density * 0.8);
            case Mode::ladder: return grid * (0.7 - parameters.density * 0.35);
            case Mode::smear: return grid * (0.75 - parameters.density * 0.4);
            case Mode::grid: return grid;
            case Mode::count: return grid;
            default: return grid;
        }
    }

    void renderEffect (const EngineParameters& parameters, int samples)
    {
        const auto bpm = clamp (parameters.bpm, 30.0, 300.0);
        const auto quarter = sampleRate * 60.0 / bpm;
        const auto grid = clamp (quarter * divisionFactor (parameters.division), 32.0, sampleRate * 4.0);
        if (parameters.seed != previousSeed) { sequenceStep = 0; spawnCountdown = 0; previousSeed = parameters.seed; }
        if (parameters.hostPositionValid && parameters.hostPlaying)
        {
            if (! wasHostPlaying || std::abs (parameters.hostPpq - expectedPpq) > .02)
            {
                const auto interval = std::max (12., intervalFor (parameters, grid));
                const auto timeline = parameters.hostPpq * quarter;
                spawnCountdown = std::fmod (interval - std::fmod (timeline, interval), interval);
                sequenceStep = std::max (0, static_cast<int> (std::floor (timeline / interval)));
                for (auto& grain : grains) grain.active = false;
            }
            expectedPpq = parameters.hostPpq + samples / quarter;
        }
        wasHostPlaying = parameters.hostPositionValid && parameters.hostPlaying;
        auto material = parameters;
        const auto friction = 1.f - std::exp (-1.f / static_cast<float> (sampleRate * (.004 + parameters.viscosity * .6)));
        const auto attack = 1.f - std::exp (-1.f / static_cast<float> (sampleRate * parameters.magnetAttack));
        const auto magnetRelease = 1.f - std::exp (-1.f / static_cast<float> (sampleRate * parameters.magnetRelease));
        auto blockInputPeak = 0.0f;
        auto blockEffectPeak = 0.0f;
        auto countedActiveGrains = 0;

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto inputL = parameters.bypass && parameters.trails ? 0.f : stage[0][static_cast<std::size_t> (sample)];
            const auto inputR = parameters.bypass && parameters.trails ? 0.f : stage[1][static_cast<std::size_t> (sample)];
            const auto side = parameters.sidechain ? std::abs (parameters.sidechain[sample]) : 0.f;
            const auto previousMagnet = magnetEnvelope;
            magnetEnvelope += (side > magnetEnvelope ? attack : magnetRelease) * (side - magnetEnvelope);
            const auto magnetic = std::tanh (magnetEnvelope * 5.f) * parameters.magnetAmount;
            fieldPosition += friction * (parameters.fieldPosition - fieldPosition);
            fieldPitch += friction * (parameters.fieldPitch - fieldPitch);
            fieldStretch += friction * (parameters.fieldStretch - fieldStretch);
            material.fieldPosition = fieldPosition;
            material.fieldPitch = fieldPitch;
            material.fieldStretch = fieldStretch * (parameters.magnetMode == 0 ? 1.f - magnetic * .65f : 1.f);
            material.cohesion = clamp (parameters.cohesion + (parameters.magnetMode == 2 ? magnetic : 0.f), 0.f, 1.f);
            blockInputPeak = std::max ({ blockInputPeak, std::abs (inputL), std::abs (inputR) });

            if (! parameters.freeze)
                for (auto& age : onsetAges)
                    if (age >= 0.0) age += 1.0;

            const auto onsetHit = ! parameters.freeze && onset.process (inputL, inputR);
            if (onsetHit)
                rememberOnset();

            if (! parameters.freeze)
            {
                const auto feedbackAmount = parameters.repeats * 0.48f;
                capture.write (saturate (inputL + feedbackL * feedbackAmount),
                               saturate (inputR + feedbackR * feedbackAmount));
            }

            spawnCountdown -= 1.0;
            auto shouldSpawn = spawnCountdown <= 0.0;
            if (! parameters.patternLock && onsetHit && (parameters.mode == Mode::pluck
                             || parameters.mode == Mode::chop
                             || parameters.mode == Mode::breakUp
                             || parameters.mode == Mode::ladder))
                shouldSpawn = true;

            if (parameters.magnetMode == 1 && previousMagnet < .08f && magnetEnvelope >= .08f && parameters.magnetAmount > 0.f) shouldSpawn = true;
            if (shouldSpawn && ! parameters.looperOnly && ! (parameters.bypass && parameters.trails))
            {
                spawnEvent (parameters, grid, onsetHit);
                spawnCountdown = std::max (intervalFor (parameters, grid) * (parameters.rhythmMutation != 0 ? .5 + rhythmValue (parameters) : 1.0), 12.0);
            }

            float effectL = 0.0f;
            float effectR = 0.0f;
            auto active = 0;

            if (parameters.mode == Mode::grid || parameters.mode == Mode::smear)
                delay.process (inputL, inputR, material, effectL, effectR);

            if (parameters.mode != Mode::grid)
            {
                float grainL = 0.0f;
                float grainR = 0.0f;
                for (auto& grain : grains)
                {
                    grain.process (capture, ! parameters.freeze, material, grainL, grainR);
                    if (grain.active)
                        ++active;
                }
                const auto normalizer = active > 0 ? 1.0f / std::sqrt (1.0f + 0.12f * static_cast<float> (active)) : 1.0f;
                if (parameters.mode == Mode::smear)
                {
                    effectL = effectL * 0.68f + grainL * 0.62f * normalizer;
                    effectR = effectR * 0.68f + grainR * 0.62f * normalizer;
                }
                else
                {
                    effectL = grainL * normalizer;
                    effectR = grainR * normalizer;
                }
            }

            if (parameters.variation == 3
                && (parameters.mode == Mode::chain
                    || parameters.mode == Mode::chop
                    || parameters.mode == Mode::ladder))
            {
                const auto steps = static_cast<float> (1 << clamp (12 - static_cast<int> (parameters.density * 7.0f), 5, 12));
                effectL = std::round (effectL * steps) / steps;
                effectR = std::round (effectR * steps) / steps;
            }

            if (parameters.looperOnly) { effectL = inputL; effectR = inputR; }
            if (parameters.magnetMode == 0) { effectL *= 1.f - magnetic * .8f; effectR *= 1.f - magnetic * .8f; }
            reverseCloud.process (effectL, effectR, parameters.reverse, bpm, parameters.division);
            modulation.process (effectL, effectR, parameters.modulationDepth, parameters.modulationRateHz);

            auto targetCutoff = parameters.cutoffHz;
            if (parameters.mode == Mode::orbit && (parameters.variation == 1 || parameters.variation == 3))
                targetCutoff *= .25f + .75f * (.5f + .5f * std::sin (static_cast<float> (sampleClock + sample) / static_cast<float> (quarter) * pi));
            smoothedCutoff += 0.0015f * (targetCutoff - smoothedCutoff);
            filterL.set (smoothedCutoff, parameters.resonance);
            filterR.set (smoothedCutoff, parameters.resonance);
            effectL = filterL.process (effectL);
            effectR = filterR.process (effectR);

            float reverbL = 0.0f;
            float reverbR = 0.0f;
            reverb.process (effectL, effectR, parameters.space, parameters.reverbStyle, reverbL, reverbR);
            const auto dryEffect = 1.0f - parameters.space * 0.3f;
            effectL = effectL * dryEffect + reverbL * parameters.space * 1.18f;
            effectR = effectR * dryEffect + reverbR * parameters.space * 1.18f;

            feedbackL = saturate (effectL * 0.82f);
            feedbackR = saturate (effectR * 0.82f);
            smoothedMix += 0.0015f * (parameters.mix - smoothedMix);
            smoothedOutput += 0.0015f * (parameters.outputGain - smoothedOutput);
            const auto dryGain = std::cos (clamp (smoothedMix, 0.0f, 1.0f) * pi * 0.5f);
            const auto wetGain = std::sin (clamp (smoothedMix, 0.0f, 1.0f) * pi * 0.5f);
            wet[0][static_cast<std::size_t> (sample)] = (inputL * dryGain + effectL * wetGain) * smoothedOutput;
            wet[1][static_cast<std::size_t> (sample)] = (inputR * dryGain + effectR * wetGain) * smoothedOutput;
            blockEffectPeak = std::max ({ blockEffectPeak, std::abs (effectL), std::abs (effectR) });
            countedActiveGrains = std::max (countedActiveGrains, active);
        }

        const auto release = std::exp (-static_cast<float> (samples / (sampleRate * 0.18)));
        inputLevel.store (std::max (blockInputPeak, inputLevel.load() * release), std::memory_order_relaxed);
        effectLevel.store (std::max (blockEffectPeak, effectLevel.load() * release), std::memory_order_relaxed);
        activeGrains.store (countedActiveGrains, std::memory_order_relaxed);
    }

    void process (const float* const* inputs,
                  float* const* outputs,
                  int channels,
                  int samples,
                  const EngineParameters& incoming)
    {
        auto parameters = incoming;
        if (parameters.bypass && parameters.trails) parameters.freeze = false;
        if (samples <= 0 || inputs == nullptr || outputs == nullptr)
            return;
        if (samples > maximumBlockSize)
        {
            const auto actualChannels = clamp (channels, 1, 2);
            for (int offset = 0; offset < samples; offset += maximumBlockSize)
            {
                const float* chunkIn[] { inputs[0] + offset, inputs[actualChannels - 1] + offset };
                float* chunkOut[] { outputs[0] + offset, outputs[actualChannels - 1] + offset };
                auto chunkParameters = parameters;
                chunkParameters.hostPpq += offset / (sampleRate * 60.0 / parameters.bpm);
                if (parameters.sidechain) chunkParameters.sidechain += offset;
                process (chunkIn, chunkOut, actualChannels, std::min (maximumBlockSize, samples - offset), chunkParameters);
            }
            return;
        }

        looper.beginBlock();
        auto loopParameters = parameters;
        if (parameters.bypass && parameters.trails)
        {
            loopParameters.loopFade = std::max (.05f, parameters.loopFade); loopParameters.fadeMode = 0; loopParameters.quantize = false;
            if (! wasTrailsBypassed) looper.request (LooperCommand::stop);
        }
        wasTrailsBypassed = parameters.bypass && parameters.trails;
        const auto actualChannels = clamp (channels, 1, 2);
        for (int sample = 0; sample < samples; ++sample)
        {
            dry[0][static_cast<std::size_t> (sample)] = inputs[0][sample];
            dry[1][static_cast<std::size_t> (sample)] = actualChannels > 1 ? inputs[1][sample] : inputs[0][sample];
        }

        if (parameters.looperBeforeEffect)
        {
            for (int sample = 0; sample < samples; ++sample)
                looper.process (dry[0][static_cast<std::size_t> (sample)],
                                dry[1][static_cast<std::size_t> (sample)],
                                loopParameters, sample,
                                stage[0][static_cast<std::size_t> (sample)],
                                stage[1][static_cast<std::size_t> (sample)]);
        }
        else
        {
            std::copy_n (dry[0].begin(), samples, stage[0].begin());
            std::copy_n (dry[1].begin(), samples, stage[1].begin());
        }

        renderEffect (parameters, samples);

        if (! parameters.looperBeforeEffect)
        {
            for (int sample = 0; sample < samples; ++sample)
            {
                float loopedL = 0.0f;
                float loopedR = 0.0f;
                looper.process (wet[0][static_cast<std::size_t> (sample)],
                                wet[1][static_cast<std::size_t> (sample)],
                                loopParameters, sample,
                                loopedL,
                                loopedR);
                wet[0][static_cast<std::size_t> (sample)] = loopedL;
                wet[1][static_cast<std::size_t> (sample)] = loopedR;
            }
        }

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto index = static_cast<std::size_t> (sample);
            smoothedBypass += 0.003f * ((parameters.bypass && ! parameters.trails ? 1.0f : 0.0f) - smoothedBypass);
            outputs[0][sample] = lerp (wet[0][index], dry[0][index], smoothedBypass);
            if (actualChannels > 1)
                outputs[1][sample] = lerp (wet[1][index], dry[1][index], smoothedBypass);
            if (parameters.bypass && parameters.trails)
            { outputs[0][sample] += dry[0][index]; if (actualChannels > 1) outputs[1][sample] += dry[1][index]; }
            const auto historyIndex = historyClock.load (std::memory_order_relaxed) % static_cast<std::uint64_t> (historySize);
            history[0][historyIndex].store (outputs[0][sample], std::memory_order_relaxed);
            history[1][historyIndex].store (outputs[actualChannels - 1][sample], std::memory_order_relaxed);
            historyClock.fetch_add (1, std::memory_order_release);
            outputPeak = std::max ({ outputPeak * 0.99988f, std::abs (outputs[0][sample]),
                                     std::abs (outputs[actualChannels - 1][sample]) });
            std::array<float, 3> bandPower {};
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                const auto signal = outputs[std::min (static_cast<int> (channel), actualChannels - 1)][sample];
                visualLow[channel] += visualLowCoefficient * (signal - visualLow[channel]);
                visualUpper[channel] += visualUpperCoefficient * (signal - visualUpper[channel]);
                const std::array bands { visualLow[channel], visualUpper[channel] - visualLow[channel], signal - visualUpper[channel] };
                for (std::size_t band = 0; band < bands.size(); ++band) bandPower[band] += bands[band] * bands[band] * .5f;
            }
            for (std::size_t band = 0; band < bandPower.size(); ++band)
                visualBandPower[band] += (bandPower[band] - visualBandPower[band]) * (bandPower[band] > visualBandPower[band] ? visualAttack : visualRelease);
            // Fixed storage only; collect the actual post-mix/post-bypass signal.
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                const auto amplitude = outputs[std::min (static_cast<int> (channel), actualChannels - 1)][sample];
                scopeLow[channel] = std::min (scopeLow[channel], amplitude);
                scopeHigh[channel] = std::max (scopeHigh[channel], amplitude);
            }
            if (++scopeSamples >= scopeBinSamples)
            {
                for (std::size_t channel = 0; channel < 2; ++channel)
                {
                    outputLow[channel][scopeWrite] = scopeLow[channel];
                    outputHigh[channel][scopeWrite] = scopeHigh[channel];
                }
                scopeWrite = (scopeWrite + 1u) % VisualFrame::outputPoints;
                scopeSamples = 0;
                scopeLow = scopeHigh = {};
            }
        }
        sampleClock += static_cast<std::uint64_t> (samples);
        visualCountdown -= samples;
        if (visualCountdown <= 0)
        {
            visualCountdown = std::max (1, static_cast<int> (sampleRate / 60.0));
            publishVisual (parameters);
        }
    }

    void publishVisual (const EngineParameters& parameters) noexcept
    {
        const auto write = visualWrite.load (std::memory_order_relaxed);
        const auto next = (write + 1u) % visualFrames.size();
        if (next == visualRead.load (std::memory_order_acquire)) return;
        auto& frame = visualFrames[write];
        frame = {};
        frame.sampleTime = sampleClock;
        frame.mode = parameters.mode;
        frame.variation = parameters.variation;
        frame.bpm = parameters.bpm;
        frame.viscosity = parameters.viscosity; frame.cohesion = parameters.cohesion; frame.tension = parameters.tension;
        frame.magnet = std::tanh (magnetEnvelope * 5.f) * parameters.magnetAmount;
        frame.fieldPosition = fieldPosition; frame.fieldPitch = fieldPitch; frame.fieldStretch = fieldStretch; frame.fieldSplit = parameters.fieldSplit;
        frame.held = parameters.freeze;
        frame.reverse = parameters.reverse;
        frame.bypass = parameters.bypass;
        frame.inputLevel = inputLevel.load (std::memory_order_relaxed);
        frame.effectLevel = effectLevel.load (std::memory_order_relaxed);
        frame.outputLevel = outputPeak;
        for (std::size_t band = 0; band < visualBandPower.size(); ++band) frame.spectralEnergy[band] = std::sqrt (std::max (0.f, visualBandPower[band]));
        for (std::size_t channel = 0; channel < 2; ++channel)
            for (std::size_t point = 0; point < VisualFrame::outputPoints; ++point)
            {
                const auto index = (scopeWrite + point) % VisualFrame::outputPoints;
                frame.outputLow[channel][point] = outputLow[channel][index];
                frame.outputHigh[channel][point] = outputHigh[channel][index];
            }
        if (parameters.mode != Mode::grid)
            for (const auto& grain : grains)
            {
                if (! grain.active || grain.level < 0.00005f) continue;
                auto& voice = frame.voices[static_cast<std::size_t> (frame.voiceCount++)];
                voice.id = grain.id;
                voice.phase = grain.age / grain.duration;
                voice.duration = grain.duration / static_cast<float> (sampleRate);
                voice.rate = static_cast<float> (grain.rate * std::pow (2., fieldPitch / 12.));
                voice.pan = grain.pan;
                voice.envelope = std::pow (std::max (0.0f, std::sin (pi * voice.phase)), clamp (grain.envelopePower + (parameters.tension - .5f) * 3.f, .2f, 6.f));
                voice.envelopePower = clamp (grain.envelopePower + (parameters.tension - .5f) * 3.f, .2f, 6.f);
                voice.level = grain.level;
                for (std::size_t point = 0; point < voice.waveform.size(); ++point)
                {
                    const auto offset = (static_cast<double> (point) / static_cast<double> (voice.waveform.size() - 1) - 0.5)
                                        * std::min (static_cast<double> (grain.duration), sampleRate * 0.06);
                    voice.waveform[point] = capture.read (0, grain.delay - offset * grain.rate);
                }
            }
        if (parameters.mode == Mode::grid || parameters.mode == Mode::smear)
            delay.appendVisuals (frame, parameters.density);
        looper.copyVisual (frame);
        visualWrite.store (next, std::memory_order_release);
    }

    bool readVisual (VisualFrame& destination) noexcept
    {
        auto read = visualRead.load (std::memory_order_relaxed);
        const auto write = visualWrite.load (std::memory_order_acquire);
        if (read == write) return false;
        while (read != write)
        {
            destination = visualFrames[read];
            read = (read + 1u) % visualFrames.size();
        }
        visualRead.store (read, std::memory_order_release);
        return true;
    }

    double sampleRate = 44100.0;
    int maximumBlockSize = 512;
    std::array<std::unique_ptr<std::atomic<float>[]>, 2> history;
    std::atomic<std::uint64_t> historyClock { 0 };
    int historySize = 1, previousSeed = -1;
    float fieldPosition = 0.f, fieldPitch = 0.f, fieldStretch = 1.f, magnetEnvelope = 0.f;
    bool wasTrailsBypassed = false;
    bool wasHostPlaying = false; double expectedPpq = 0;
    StereoRing capture;
    MultiTapDelay delay;
    ModulatedDelay modulation;
    ReverseCloud reverseCloud;
    StereoReverb reverb;
    StateVariableLowPass filterL;
    StateVariableLowPass filterR;
    OnsetDetector onset;
    PhraseLooper looper;
    std::array<Grain, maximumGrains> grains;
    std::array<double, 8> onsetAges {};
    std::array<std::vector<float>, 2> dry;
    std::array<std::vector<float>, 2> stage;
    std::array<std::vector<float>, 2> wet;
    Random random, pitchRandom;
    int onsetCount = 0;
    int sequenceStep = 0, eventStep = 0;
    double spawnCountdown = 0.0;
    float feedbackL = 0.0f;
    float feedbackR = 0.0f;
    float smoothedMix = 0.5f;
    float smoothedCutoff = 18000.0f;
    float smoothedOutput = 1.0f;
    float smoothedBypass = 0.0f, outputPeak = 0.0f;
    std::array<float, 2> visualLow {}, visualUpper {};
    std::array<float, 3> visualBandPower {};
    float visualLowCoefficient = 0.f, visualUpperCoefficient = 0.f, visualAttack = 0.f, visualRelease = 0.f;
    std::uint64_t sampleClock = 0, voiceSequence = 0;
    int visualCountdown = 0;
    std::array<std::array<float, VisualFrame::outputPoints>, 2> outputLow {}, outputHigh {};
    std::array<float, 2> scopeLow {}, scopeHigh {};
    std::size_t scopeWrite = 0;
    int scopeSamples = 0, scopeBinSamples = 80;
    std::array<VisualFrame, 8> visualFrames {};
    std::atomic<std::size_t> visualRead { 0 }, visualWrite { 0 };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> effectLevel { 0.0f };
    std::atomic<int> activeGrains { 0 };
};

Engine::Engine() : impl (std::make_unique<Impl>()) {}
Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int maximumBlockSize, int channels)
{
    impl->prepare (sampleRate, maximumBlockSize, channels);
}

void Engine::reset()
{
    impl->reset();
}

void Engine::process (const float* const* inputs,
                      float* const* outputs,
                      int channels,
                      int samples,
                      const EngineParameters& parameters)
{
    impl->process (inputs, outputs, channels, samples, parameters);
}

AudioSnapshot Engine::snapshotHistory (double seconds)
{
    AudioSnapshot snapshot; snapshot.sampleRate = impl->sampleRate; snapshot.playing = true;
    const auto end = impl->historyClock.load (std::memory_order_acquire);
    const auto count = std::min ({ end, static_cast<std::uint64_t> (impl->historySize), static_cast<std::uint64_t> (std::max (0.0, seconds) * impl->sampleRate) });
    for (auto& c : snapshot.audio) c.resize (static_cast<std::size_t> (count));
    for (std::uint64_t i = 0; i < count; ++i) for (int c = 0; c < 2; ++c)
        snapshot.audio[c][i] = impl->history[c][(end - count + i) % static_cast<std::uint64_t> (impl->historySize)].load (std::memory_order_relaxed);
    return snapshot;
}
AudioSnapshot Engine::snapshotLoop() { return impl->looper.snapshot(); }
bool Engine::restoreLoop (const AudioSnapshot& audio) { return impl->looper.restore (audio); }

void Engine::requestLooperCommand (LooperCommand command) noexcept
{
    impl->looper.request (command);
}

LooperState Engine::getLooperState() const noexcept
{
    return impl->looper.currentState();
}

float Engine::getLooperProgress() const noexcept
{
    return impl->looper.currentProgress();
}

float Engine::getInputLevel() const noexcept
{
    return impl->inputLevel.load (std::memory_order_relaxed);
}

float Engine::getEffectLevel() const noexcept
{
    return impl->effectLevel.load (std::memory_order_relaxed);
}

int Engine::getActiveGrainCount() const noexcept
{
    return impl->activeGrains.load (std::memory_order_relaxed);
}

bool Engine::readVisualFrame (VisualFrame& destination) noexcept
{
    return impl->readVisual (destination);
}
} // namespace motefield
