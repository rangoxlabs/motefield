#include "DSP.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{
constexpr double rate = 48000.0;
constexpr int block = 480; // Analysis frames of exactly 10 ms.
constexpr int frames = 500;
constexpr double onsetTime = 1.0;
constexpr double pi = 3.14159265358979323846;

double source (int sample, int kind)
{
    const double t = sample / rate - onsetTime;
    if (t < 0.0 || kind == 2) return 0.0;
    double envelope = 0.0;
    if (kind == 0 && t < 0.8)
        envelope = std::min (1.0, t / .005) * std::exp (-t * 7.0)
                   * std::min (1.0, (.8 - t) / .02);
    if (kind == 1 && t < 3.0)
        envelope = .5 - .5 * std::cos (2.0 * pi * t / 3.0);
    // Harmonic, deterministic, mono-compatible source; no stereo cancellation in the fixture.
    return envelope * (std::sin (2.0 * pi * 220.0 * t)
                       + .2 * std::sin (2.0 * pi * 440.0 * t)) / 1.2;
}

double db (double amplitude) { return 20.0 * std::log10 (std::max (amplitude, 1.0e-12)); }
}

int main (int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: MoteFieldResponseBench results.csv\n"; return 2; }
    std::ofstream csv (argv[1]);
    if (! csv) { std::cerr << "Cannot open output file\n"; return 2; }
    csv << "mode,variation,source,input_gain_db,input_peak_dbfs,wet_peak_dbfs,wet_rms_dbfs,"
           "wet_to_input_rms_db,first_response_ms,active_frames_pct,finite\n";
    csv << std::fixed << std::setprecision (4);
    bool valid = true;
    int cases = 0;
    constexpr std::array<const char*, 3> names { "plucked_note", "slow_swell", "silence" };
    for (int mode = 0; mode < static_cast<int> (motefield::Mode::count); ++mode)
        for (int variation = 0; variation < 4; ++variation)
            for (int kind = 0; kind < 3; ++kind)
                for (int level = 0; level < (kind == 2 ? 1 : 2); ++level)
                {
                    const double gainDb = level == 0 ? -12.0 : -36.0;
                    const double gain = std::pow (10.0, gainDb / 20.0);
                    std::array<float, block> inL {}, inR {}, outL {}, outR {};
                    const float* inputs[] { inL.data(), inR.data() };
                    float* outputs[] { outL.data(), outR.data() };
                    motefield::Engine engine;
                    engine.prepare (rate, block, 2);
                    motefield::EngineParameters p;
                    p.mode = static_cast<motefield::Mode> (mode);
                    p.variation = variation;
                    p.mix = 1.0f;
                    p.hostPositionValid = true;
                    p.hostPlaying = true;
                    p.bpm = 120.0;
                    // Otherwise use EngineParameters defaults: these are controlled mode
                    // comparisons, not factory-preset scores. No level matching or limiter.
                    double inputPeak = 0.0, wetPeak = 0.0, inputEnergy = 0.0, wetEnergy = 0.0;
                    std::array<double, frames> frameRms {};
                    bool finite = true;
                    for (int frame = 0; frame < frames; ++frame)
                    {
                        for (int s = 0; s < block; ++s)
                        {
                            inL[s] = static_cast<float> (gain * source (frame * block + s, kind));
                            inR[s] = inL[s] * .91f;
                            inputPeak = std::max (inputPeak, static_cast<double> (std::abs (inL[s])));
                            inputEnergy += .5 * (inL[s] * inL[s] + inR[s] * inR[s]);
                        }
                        p.hostPpq = frame * block / rate * p.bpm / 60.0;
                        engine.process (inputs, outputs, 2, block, p);
                        double energy = 0.0;
                        for (int s = 0; s < block; ++s)
                        {
                            finite = finite && std::isfinite (outL[s]) && std::isfinite (outR[s]);
                            wetPeak = std::max ({ wetPeak, static_cast<double> (std::abs (outL[s])),
                                                static_cast<double> (std::abs (outR[s])) });
                            energy += .5 * (outL[s] * outL[s] + outR[s] * outR[s]);
                        }
                        frameRms[frame] = std::sqrt (energy / block);
                        wetEnergy += energy;
                    }
                    // Diagnostic only: >=30 ms continuously above -30 dB relative to
                    // measured input peak. It is not perceptual latency or a quality score.
                    int consecutive = 0, firstFrame = -1, active = 0;
                    const double threshold = inputPeak * std::pow (10.0, -30.0 / 20.0);
                    for (int frame = static_cast<int> (onsetTime * 100); frame < frames; ++frame)
                    {
                        const bool audible = inputPeak > 0.0 && frameRms[frame] >= threshold;
                        if (audible) ++active;
                        consecutive = audible ? consecutive + 1 : 0;
                        if (firstFrame < 0 && consecutive == 3) firstFrame = frame - 2;
                    }
                    csv << motefield::modeNames[mode] << ',' << static_cast<char> ('A' + variation)
                        << ',' << names[kind] << ',' << gainDb << ',' << db (inputPeak) << ','
                        << db (wetPeak) << ',' << db (std::sqrt (wetEnergy / (frames * block))) << ',';
                    if (inputEnergy > 0.0) csv << db (std::sqrt (wetEnergy / inputEnergy));
                    csv << ',';
                    if (firstFrame >= 0) csv << firstFrame * 10.0 - onsetTime * 1000.0;
                    csv << ',' << active * 100.0 / (frames - onsetTime * 100) << ',' << finite << '\n';
                    valid = valid && finite && (kind != 2 || wetPeak < 1.0e-7);
                    ++cases;
                }
    csv.close();
    if (! csv) return 2;
    std::cout << cases << " response cases written. Finite output and silence checks: "
              << (valid ? "PASS" : "FAIL") << '\n';
    return valid ? 0 : 1;
}
