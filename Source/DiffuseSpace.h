#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace motefield
{
// Eight delay paths with an energy-preserving Householder feedback matrix.
// Audition sums are independent of the recirculating field.
class DiffuseSpace
{
public:
    void prepare (double rate)
    {
        sampleRate = rate; capacity = static_cast<int> (std::ceil (rate * .65));
        highPass = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979323846 * 90.0 / rate));
        for (auto& line : lines) line.assign (capacity, 0.f);
        reset();
    }
    void reset()
    {
        for (auto& line : lines) std::fill (line.begin(), line.end(), 0.f);
        damp = {}; bass = {}; delays = {}; targets = {};
        inputBass = {}; outputTone = {}; cursor = tick = 0; phase = 0.; styleSmooth = 1.5f;
    }
    void process (float l, float r, float amount, int style, float cutoff, float& outL, float& outR) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr std::array<float, 8> times { .0311f,.0437f,.0569f,.0673f,.0797f,.0911f,.1073f,.1237f };
        constexpr std::array<float, 4> sizes { .8f,1.3f,2.1f,3.6f };
        constexpr std::array<float, 4> decays { .67f,.76f,.84f,.92f };
        constexpr std::array<float, 4> tones { 12500.f,3500.f,9000.f,6500.f };
        style = std::clamp (style, 0, 3);
        const float hp = highPass;
        inputBass[0] += hp * (l - inputBass[0]); inputBass[1] += hp * (r - inputBass[1]);
        l -= inputBass[0]; r -= inputBass[1];
        styleSmooth += .0003f * (sizes[style] - styleSmooth);
        if ((tick++ & 63) == 0)
        {
            phase += 64.0 / sampleRate;
            const auto frequency = std::clamp (std::min (cutoff, tones[style]), 150.f, static_cast<float> (sampleRate * .4));
            damping = static_cast<float> (1.0 - std::exp (-2.0 * pi * frequency / sampleRate));
            for (std::size_t i = 0; i < lines.size(); ++i)
                targets[i] = static_cast<float> (sampleRate * (times[i] * styleSmooth
                    + .00035 * (1.0 + style * .3) * std::sin (phase * (0.7 + i * .11) + i)));
        }
        std::array<float, 8> taps {}; float sum = 0.f;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (delays[i] == 0.f) delays[i] = targets[i];
            delays[i] += .002f * (targets[i] - delays[i]);
            double position = cursor - std::clamp (delays[i], 2.f, static_cast<float> (capacity - 2));
            if (position < 0.) position += capacity;
            const int a = static_cast<int> (position), b = (a + 1) % capacity;
            const float value = lines[i][a] + static_cast<float> (position - a) * (lines[i][b] - lines[i][a]);
            damp[i] += damping * (value - damp[i]);
            bass[i] += hp * .5f * (damp[i] - bass[i]);
            taps[i] = damp[i] - bass[i] * .35f; sum += taps[i];
        }
        const float feedback = decays[style] * (.88f + std::clamp (amount,0.f,1.f) * .1f);
        for (std::size_t i = 0; i < lines.size(); ++i)
            lines[i][cursor] = std::tanh ((i & 1 ? r : l) * .19f + (sum * .25f - taps[i]) * feedback);
        outL = (taps[0] + taps[2] + taps[5] - taps[7]) * .38f;
        outR = (taps[1] + taps[3] + taps[4] - taps[6]) * .38f;
        // Tail tone follows Filter as well as Reverb Character, including stored energy.
        outputTone[0] += damping * (outL - outputTone[0]);
        outputTone[1] += damping * (outR - outputTone[1]);
        outL = outputTone[0]; outR = outputTone[1]; cursor = (cursor + 1) % capacity;
    }
private:
    std::array<std::vector<float>, 8> lines;
    std::array<float, 8> damp {}, bass {}, delays {}, targets {};
    std::array<float, 2> inputBass {}, outputTone {};
    double sampleRate = 44100., phase = 0.;
    int capacity = 1, cursor = 0, tick = 0;
    float damping = .3f, styleSmooth = 1.5f;
    float highPass = .01f;
};
}
