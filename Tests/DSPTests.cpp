#include "DSP.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <vector>
#include <thread>

namespace { thread_local bool watchingAudioAllocations = false; thread_local int audioAllocations = 0; }
void* operator new (std::size_t size)
{
    if (watchingAudioAllocations) ++audioAllocations;
    if (auto* memory = std::malloc (std::max (std::size_t { 1 }, size))) return memory;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size) { return ::operator new (size); }
void operator delete (void* memory) noexcept { std::free (memory); }
void operator delete[] (void* memory) noexcept { std::free (memory); }
void operator delete (void* memory, std::size_t) noexcept { std::free (memory); }
void operator delete[] (void* memory, std::size_t) noexcept { std::free (memory); }

namespace
{
constexpr double sampleRate = 48000.0;
constexpr float twoPi = 6.28318530717958647692f;

void require (bool condition, const std::string& message)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit (1);
    }
}

struct Render
{
    std::vector<float> left;
    std::vector<float> right;
    double energy = 0.0;
    float peak = 0.0f;
};

float testSignal (int sample)
{
    const auto time = static_cast<float> (sample / sampleRate);
    const auto withinHit = std::fmod (time, 0.25f);
    if (time > 1.45f || withinHit > 0.085f)
        return 0.0f;
    const auto envelope = std::exp (-withinHit * 34.0f);
    const auto frequency = 110.0f * std::pow (2.0f, std::floor (time / 0.25f) / 12.0f);
    return 0.42f * envelope * (std::sin (twoPi * frequency * time)
                               + 0.24f * std::sin (twoPi * frequency * 2.01f * time));
}

Render renderMode (motefield::Mode mode, int variation, int blockSize, bool reverse = false, int preparedSize = 0)
{
    constexpr int totalSamples = static_cast<int> (sampleRate * 2.8);
    motefield::Engine engine;
    engine.prepare (sampleRate, preparedSize > 0 ? preparedSize : blockSize, 2);
    motefield::EngineParameters parameters;
    parameters.mode = mode;
    parameters.variation = variation;
    parameters.density = 0.72f;
    parameters.repeats = 0.68f;
    parameters.shape = 0.56f;
    parameters.division = 4;
    parameters.bpm = 120.0;
    parameters.reverse = reverse;
    parameters.modulationDepth = 0.12f;
    parameters.modulationRateHz = 0.4f;
    parameters.cutoffHz = 15500.0f;
    parameters.resonance = 0.22f;
    parameters.space = 0.28f;
    parameters.reverbStyle = 2;
    parameters.mix = 1.0f;

    Render render;
    render.left.resize (totalSamples);
    render.right.resize (totalSamples);
    std::vector<float> inputL (static_cast<std::size_t> (blockSize));
    std::vector<float> inputR (static_cast<std::size_t> (blockSize));
    std::vector<float> outputL (static_cast<std::size_t> (blockSize));
    std::vector<float> outputR (static_cast<std::size_t> (blockSize));

    for (int position = 0; position < totalSamples; position += blockSize)
    {
        const auto count = std::min (blockSize, totalSamples - position);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto value = testSignal (position + sample);
            inputL[static_cast<std::size_t> (sample)] = value;
            inputR[static_cast<std::size_t> (sample)] = value * 0.91f;
        }

        const float* inputs[] { inputL.data(), inputR.data() };
        float* outputs[] { outputL.data(), outputR.data() };
        engine.process (inputs, outputs, 2, count, parameters);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto left = outputL[static_cast<std::size_t> (sample)];
            const auto right = outputR[static_cast<std::size_t> (sample)];
            render.left[static_cast<std::size_t> (position + sample)] = left;
            render.right[static_cast<std::size_t> (position + sample)] = right;
            require (std::isfinite (left) && std::isfinite (right), "DSP emitted NaN or infinity");
            render.energy += static_cast<double> (left * left + right * right);
            render.peak = std::max ({ render.peak, std::abs (left), std::abs (right) });
        }
    }
    return render;
}

void testAllModesAndVariations()
{
    for (int mode = 0; mode < static_cast<int> (motefield::Mode::count); ++mode)
    {
        for (int variation = 0; variation < 4; ++variation)
        {
            const auto render = renderMode (static_cast<motefield::Mode> (mode), variation, 193);
            require (render.energy > 0.001, std::string (motefield::modeNames[static_cast<std::size_t> (mode)])
                                             + " variation " + std::to_string (variation + 1) + " was silent");
            require (render.peak < 16.0f, std::string (motefield::modeNames[static_cast<std::size_t> (mode)])
                                           + " variation " + std::to_string (variation + 1) + " was unstable");
        }
    }
}

void testBlockSizeDeterminism()
{
    const auto smallBlocks = renderMode (motefield::Mode::veil, 2, 64);
    const auto oddBlocks = renderMode (motefield::Mode::veil, 2, 257);
    require (smallBlocks.left.size() == oddBlocks.left.size(), "render lengths differ");
    auto maximumDifference = 0.0f;
    for (std::size_t i = 0; i < smallBlocks.left.size(); ++i)
    {
        maximumDifference = std::max (maximumDifference, std::abs (smallBlocks.left[i] - oddBlocks.left[i]));
        maximumDifference = std::max (maximumDifference, std::abs (smallBlocks.right[i] - oddBlocks.right[i]));
    }
    require (maximumDifference < 1.0e-5f, "output changes with host buffer size");
}

void testFreeze()
{
    constexpr int blockSize = 128;
    motefield::Engine engine;
    engine.prepare (sampleRate, blockSize, 2);
    motefield::EngineParameters parameters;
    parameters.mode = motefield::Mode::veil;
    parameters.variation = 1;
    parameters.density = 0.86f;
    parameters.repeats = 0.72f;
    parameters.mix = 1.0f;
    parameters.space = 0.35f;

    std::vector<float> inputL (blockSize), inputR (blockSize), outputL (blockSize), outputR (blockSize);
    const float* inputs[] { inputL.data(), inputR.data() };
    float* outputs[] { outputL.data(), outputR.data() };

    for (int position = 0; position < static_cast<int> (sampleRate); position += blockSize)
    {
        for (int sample = 0; sample < blockSize; ++sample)
            inputL[static_cast<std::size_t> (sample)] = inputR[static_cast<std::size_t> (sample)] = testSignal (position + sample);
        engine.process (inputs, outputs, 2, blockSize, parameters);
    }

    parameters.freeze = true;
    std::fill (inputL.begin(), inputL.end(), 0.0f);
    std::fill (inputR.begin(), inputR.end(), 0.0f);
    double frozenEnergy = 0.0;
    for (int position = 0; position < static_cast<int> (sampleRate * 0.75); position += blockSize)
    {
        engine.process (inputs, outputs, 2, blockSize, parameters);
        for (int sample = 0; sample < blockSize; ++sample)
            frozenEnergy += outputL[static_cast<std::size_t> (sample)] * outputL[static_cast<std::size_t> (sample)]
                            + outputR[static_cast<std::size_t> (sample)] * outputR[static_cast<std::size_t> (sample)];
    }
    require (frozenEnergy > 0.01, "freeze did not sustain captured audio");
}

void testPhraseLooper()
{
    constexpr int blockSize = 128;
    motefield::Engine engine;
    engine.prepare (sampleRate, blockSize, 2);
    motefield::EngineParameters parameters;
    parameters.mode = motefield::Mode::grid;
    parameters.mix = 0.0f;
    parameters.looperLevel = 0.9f;

    std::vector<float> inputL (blockSize), inputR (blockSize), outputL (blockSize), outputR (blockSize);
    const float* inputs[] { inputL.data(), inputR.data() };
    float* outputs[] { outputL.data(), outputR.data() };

    engine.requestLooperCommand (motefield::LooperCommand::recordPlayDub);
    constexpr int recordSamples = 12000;
    for (int position = 0; position < recordSamples; position += blockSize)
    {
        const auto count = std::min (blockSize, recordSamples - position);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto value = 0.25f * std::sin (twoPi * 220.0f * static_cast<float> (position + sample) / static_cast<float> (sampleRate));
            inputL[static_cast<std::size_t> (sample)] = value;
            inputR[static_cast<std::size_t> (sample)] = value;
        }
        engine.process (inputs, outputs, 2, count, parameters);
    }
    require (engine.getLooperState() == motefield::LooperState::recording, "looper did not enter record state");
    engine.requestLooperCommand (motefield::LooperCommand::recordPlayDub);
    std::fill (inputL.begin(), inputL.end(), 0.0f);
    std::fill (inputR.begin(), inputR.end(), 0.0f);
    double playbackEnergy = 0.0;
    for (int position = 0; position < recordSamples; position += blockSize)
    {
        const auto count = std::min (blockSize, recordSamples - position);
        engine.process (inputs, outputs, 2, count, parameters);
        for (int sample = 0; sample < count; ++sample)
            playbackEnergy += outputL[static_cast<std::size_t> (sample)] * outputL[static_cast<std::size_t> (sample)]
                              + outputR[static_cast<std::size_t> (sample)] * outputR[static_cast<std::size_t> (sample)];
    }
    require (engine.getLooperState() == motefield::LooperState::playing, "looper did not enter playback state");
    require (playbackEnergy > 1.0, "recorded phrase did not play back");

    engine.requestLooperCommand (motefield::LooperCommand::clear);
    engine.process (inputs, outputs, 2, blockSize, parameters);
    require (engine.getLooperState() == motefield::LooperState::empty, "looper did not clear");
}

void testOverdubAccumulationAndUndo()
{
    motefield::Engine engine;
    engine.prepare (sampleRate, 64, 2);
    motefield::EngineParameters p;
    p.mode = motefield::Mode::grid;
    p.mix = 0.0f;
    p.looperLevel = 1.0f;
    p.looperBeforeEffect = true;
    const auto run = [&] (float level, int count)
    {
        std::vector<float> input (static_cast<std::size_t> (count), level), output (static_cast<std::size_t> (count)), right (static_cast<std::size_t> (count));
        const float* in[] { input.data(), input.data() };
        float* out[] { output.data(), right.data() };
        engine.process (in, out, 2, count, p);
        motefield::VisualFrame frame;
        engine.readVisualFrame (frame);
        return output;
    };
    const auto expect = [&] (float level)
    {
        const auto rendered = run (0.0f, 512);
        for (const auto sample : rendered) require (std::abs (sample - level) < 0.0001f, "overdub history/undo lost or resurrected a layer");
    };
    run (0.0f, 8192);
    engine.requestLooperCommand (motefield::LooperCommand::record);
    run (.1f, 512);
    engine.requestLooperCommand (motefield::LooperCommand::play);
    expect (.1f);
    engine.requestLooperCommand (motefield::LooperCommand::dub);
    run (.2f, 512);
    engine.requestLooperCommand (motefield::LooperCommand::play);
    expect (.3f);
    engine.requestLooperCommand (motefield::LooperCommand::dub);
    run (0.0f, 128);
    engine.requestLooperCommand (motefield::LooperCommand::undo);
    expect (.3f);
    engine.requestLooperCommand (motefield::LooperCommand::dub);
    run (.05f, 128);
    engine.requestLooperCommand (motefield::LooperCommand::undo);
    expect (.3f);
    engine.requestLooperCommand (motefield::LooperCommand::dub);
    run (.1f, 512);
    engine.requestLooperCommand (motefield::LooperCommand::play);
    expect (.4f);
    engine.requestLooperCommand (motefield::LooperCommand::undo);
    expect (.3f);
    // Two UI commands between callbacks must both be consumed in order.
    engine.requestLooperCommand (motefield::LooperCommand::clear);
    engine.requestLooperCommand (motefield::LooperCommand::record);
    run (.15f, 129);
    require (engine.getLooperState() == motefield::LooperState::recording, "rapid looper commands were lost");
    engine.requestLooperCommand (motefield::LooperCommand::play);
    expect (.15f);
}

void testDelayHoldAndTelemetry()
{
    for (const auto mode : { motefield::Mode::grid, motefield::Mode::smear })
    {
        motefield::Engine held, free;
        held.prepare (sampleRate, 128, 2);
        free.prepare (sampleRate, 128, 2);
        motefield::EngineParameters p;
        p.mode = mode;
        p.mix = 1.0f; p.space = 0.0f; p.modulationDepth = 0.0f;
        p.repeats = 0.0f; p.bpm = 240.0; p.division = 0;
        std::array<float, 128> input {}, heldOut {}, freeOut {}, right {};
        const float* in[] { input.data(), input.data() };
        float* outA[] { heldOut.data(), right.data() };
        float* outB[] { freeOut.data(), right.data() };
        motefield::VisualFrame frame;
        held.process (in, outA, 2, 128, p);
        held.readVisualFrame (frame);
        require (frame.voiceCount == 0, "silence invented visual voices");
        free.process (in, outB, 2, 128, p);
        bool sawVoice = false;
        for (int block = 0; block < 750; ++block)
        {
            for (int i = 0; i < 128; ++i) input[static_cast<std::size_t> (i)] = .3f * std::sin (twoPi * 173.0f * static_cast<float> (block * 128 + i) / 48000.0f);
            held.process (in, outA, 2, 128, p);
            free.process (in, outB, 2, 128, p);
            if (held.readVisualFrame (frame)) sawVoice = sawVoice || frame.voiceCount > 0;
        }
        require (sawVoice, "delay audio did not produce visualization data");
        input.fill (0.0f);
        double heldEnergy = 0.0, freeEnergy = 0.0;
        for (int block = 0; block < 1500; ++block)
        {
            p.freeze = true;
            held.process (in, outA, 2, 128, p);
            p.freeze = false;
            free.process (in, outB, 2, 128, p);
            held.readVisualFrame (frame);
            if (block > 1125)
                for (int i = 0; i < 128; ++i)
                {
                    heldEnergy += heldOut[static_cast<std::size_t> (i)] * heldOut[static_cast<std::size_t> (i)];
                    freeEnergy += freeOut[static_cast<std::size_t> (i)] * freeOut[static_cast<std::size_t> (i)];
                }
        }
        require (frame.held && frame.voiceCount > 0, "held audio stopped reporting active playback");
        require (heldEnergy > 1.0, "delay Hold failed to sustain a captured phrase");
        require (freeEnergy < heldEnergy * .01, "delay Hold had no meaningful effect on tail sustain");
    }
}

void testOutputWaveformTelemetry()
{
    // Compare telemetry against the samples actually returned to a host,
    // across stereo/mono, sample rates, silence, and a full history wrap.
    for (const auto rate : { 48000.0, 96000.0 })
        for (const auto channels : { 1, 2 })
        {
            const auto samples = static_cast<int> (rate / 600.0);
            motefield::Engine engine;
            engine.prepare (rate, samples, channels);
            motefield::EngineParameters p;
            p.bypass = true;
            p.space = 0.0f;
            std::array<float, 160> left {}, right {}, outLeft {}, outRight {};
            const float* input[] { left.data(), right.data() };
            float* output[] { outLeft.data(), outRight.data() };
            std::array<std::array<float, 2>, 420> lows {}, highs {};
            int checked = 0;
            for (int block = 0; block < 420; ++block)
            {
                for (int sample = 0; sample < samples; ++sample)
                {
                    const auto time = static_cast<float> ((block * samples + sample) / rate);
                    left[static_cast<std::size_t> (sample)] = block < 24 ? 0.0f : .2f * std::sin (twoPi * 137.0f * time);
                    right[static_cast<std::size_t> (sample)] = block < 24 ? 0.0f : .07f * std::cos (twoPi * 313.0f * time);
                }
                engine.process (input, output, channels, samples, p);
                for (std::size_t channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < samples; ++sample)
                    {
                        const auto amplitude = output[std::min (static_cast<int> (channel), channels - 1)][sample];
                        lows[static_cast<std::size_t> (block)][channel] = std::min (lows[static_cast<std::size_t> (block)][channel], amplitude);
                        highs[static_cast<std::size_t> (block)][channel] = std::max (highs[static_cast<std::size_t> (block)][channel], amplitude);
                    }
                motefield::VisualFrame frame;
                if (! engine.readVisualFrame (frame)) continue;
                ++checked;
                for (std::size_t point = 0; point < motefield::VisualFrame::outputPoints; ++point)
                    for (std::size_t channel = 0; channel < 2; ++channel)
                    {
                        const auto history = block + 1 - static_cast<int> (motefield::VisualFrame::outputPoints) + static_cast<int> (point);
                        const auto low = history < 0 ? 0.0f : lows[static_cast<std::size_t> (history)][channel];
                        const auto high = history < 0 ? 0.0f : highs[static_cast<std::size_t> (history)][channel];
                        require (frame.outputLow[channel][point] == low && frame.outputHigh[channel][point] == high,
                                 "visual waveform does not match actual host output");
                    }
            }
            require (checked > 20, "output waveform telemetry did not update");
        }
    require (motefield::shapeEnvelopePower (0.0f) < motefield::shapeEnvelopePower (1.0f), "Shape contour direction changed");
}

void testSpectralTelemetry()
{
    for (const auto rate : { 48000.0, 96000.0 })
        for (int band = 0; band < 3; ++band)
        {
            motefield::Engine engine;engine.prepare (rate, 256, 2);
            motefield::EngineParameters parameters;parameters.bypass = true;
            const std::array frequencies { 65.f, 800.f, 9000.f };
            std::array<float, 256> left {}, right {}, outputLeft {}, outputRight {};
            const float* input[] { left.data(), right.data() };float* output[] { outputLeft.data(), outputRight.data() };
            motefield::VisualFrame frame;
            for (int block = 0; block < 160; ++block)
            {
                for (int sample = 0; sample < 256; ++sample)
                {
                    left[static_cast<std::size_t> (sample)] = .2f * std::sin (twoPi * frequencies[static_cast<std::size_t> (band)] * static_cast<float> ((block * 256 + sample) / rate));
                    right[static_cast<std::size_t> (sample)] = -left[static_cast<std::size_t> (sample)];
                }
                engine.process (input, output, 2, 256, parameters);engine.readVisualFrame (frame);
            }
            const auto energy = frame.spectralEnergy[static_cast<std::size_t> (band)];
            require (energy > .08f, "visual band analysis lost stereo energy");
            for (int other = 0; other < 3; ++other)
                if (other != band) require (energy > frame.spectralEnergy[static_cast<std::size_t> (other)] * 1.8f, "visual frequency bands are not distinct");
            left.fill (0.f);right.fill (0.f);
            for (int block = 0; block < 600; ++block) { engine.process (input, output, 2, 256, parameters);engine.readVisualFrame (frame); }
            for (auto value : frame.spectralEnergy) require (std::isfinite (value) && value < .00001f, "visual analysis did not settle after silence");
        }
}

void testOversizedBuffersAndBypass()
{
    const auto reference = renderMode (motefield::Mode::bloom, 1, 64);
    const auto oversized = renderMode (motefield::Mode::bloom, 1, 1027, false, 64);
    for (std::size_t i = 0; i < reference.left.size(); ++i)
        require (std::abs (reference.left[i] - oversized.left[i]) < .00001f, "oversized buffer chunking changed the audio");
    motefield::Engine engine;
    engine.prepare (sampleRate, 64, 1);
    motefield::EngineParameters p;
    p.bypass = true;
    p.mix = 1.0f;
    std::vector<float> signal (8192, .25f);
    const float* in[] { signal.data() };
    float* out[] { signal.data() };
    audioAllocations = 0;
    watchingAudioAllocations = true;
    engine.process (in, out, 1, static_cast<int> (signal.size()), p);
    watchingAudioAllocations = false;
    require (audioAllocations == 0, "audio callback allocated heap memory for an oversized buffer");
    require (std::abs (signal.back() - .25f) < .00001f, "bypass did not return dry input with in-place mono processing");
}
void testPerformanceFeatures()
{
    motefield::Engine engine; engine.prepare (sampleRate, 256, 2);
    motefield::EngineParameters p; p.mix = 0; p.looperBeforeEffect = true; p.looperLevel = 1;
    std::vector<float> in (256, .1f), l (256), r (256), side (256, .6f);
    const float* input[] { in.data(), in.data() }; float* output[] { l.data(), r.data() };
    const auto run = [&] (int count)
    { while (count > 0) { const auto n = std::min (256, count); audioAllocations = 0; watchingAudioAllocations = true;
        engine.process (input, output, 2, n, p); watchingAudioAllocations = false;
        require (audioAllocations == 0, "performance feature allocated on audio thread");
        if (p.hostPositionValid && p.hostPlaying) p.hostPpq += n / (sampleRate * 60.0 / p.bpm); count -= n; } };
    // Record and close precisely on successive quarter-note boundaries.
    p.quantize = true; p.hostPositionValid = true; p.hostPlaying = true; p.hostPpq = .25;
    engine.requestLooperCommand (motefield::LooperCommand::record); run (1000);
    require (engine.getLooperState() == motefield::LooperState::empty, "quantized record started before the beat");
    motefield::VisualFrame visual; engine.readVisualFrame (visual); require (visual.loopPending, "armed command missing from telemetry");
    run (18000); require (engine.getLooperState() == motefield::LooperState::recording, "quantized record did not start on beat");
    engine.requestLooperCommand (motefield::LooperCommand::play); run (24000);
    auto saved = engine.snapshotLoop();
    require (saved.audio[0].size() == 24000, "quantized loop is not exactly one beat");
    require (saved.playing, "saved loop lost playback state");
    require (std::abs (saved.audio[0][100] - .1f) < 1.e-6f, "snapshot changed loop samples");
    p.quantize = false; engine.requestLooperCommand (motefield::LooperCommand::clear); run (256);
    require (engine.restoreLoop (saved), "loop restore staging failed"); run (256);
    require (engine.getLooperState() == motefield::LooperState::playing, "restored loop did not resume");
    require (engine.snapshotLoop().audio[0] == saved.audio[0], "restored loop samples differ");
    // Snapshot copies may run concurrently with recording/overdubbing.
    std::atomic<bool> copying { true }; std::thread reader ([&] { for (int i = 0; i < 12; ++i) { const auto copy = engine.snapshotLoop(); require (copy.audio[0].size() == copy.audio[1].size(), "torn stereo snapshot"); } copying.store (false); });
    while (copying.load()) run (256); reader.join();
    p.loopFade = .02f; engine.requestLooperCommand (motefield::LooperCommand::stop); run (256);
    require (engine.getLooperState() == motefield::LooperState::playing, "fade-out stopped abruptly");
    run (1200); require (engine.getLooperState() == motefield::LooperState::stopped, "fade-out never stopped");
    p.loopFade = 0; engine.requestLooperCommand (motefield::LooperCommand::burstStart); run (1000);
    engine.requestLooperCommand (motefield::LooperCommand::burstEnd); run (10);
    require (engine.snapshotLoop().audio[0].size() == 1000, "burst length is wrong");
    engine.requestLooperCommand (motefield::LooperCommand::burstStart); run (500);
    engine.requestLooperCommand (motefield::LooperCommand::burstEnd); run (10);
    require (engine.snapshotLoop().audio[0].size() == 500, "new burst did not replace old phrase");
    const auto history = engine.snapshotHistory (.01);
    require (history.audio[0].size() == 480, "retrospective capture has wrong duration");
    require (std::abs (history.audio[0].back() - l[9]) < 1.e-5f, "retrospective capture is not final output");
    p.bypass = true; p.trails = true; p.freeze = true; run (4000);
    require (engine.getLooperState() == motefield::LooperState::stopped, "trails bypass left the phrase running indefinitely");
    engine.readVisualFrame (visual); require (! visual.held, "trails bypass did not release Hold");
    p.bypass = p.trails = p.freeze = false;
    engine.requestLooperCommand (motefield::LooperCommand::clear); run (256);
    p.quantize = true; p.hostPpq = .25; engine.requestLooperCommand (motefield::LooperCommand::record); run (256);
    engine.requestLooperCommand (motefield::LooperCommand::burstStart); run (256); engine.readVisualFrame (visual);
    run (20000); require (engine.getLooperState() == motefield::LooperState::recording, "stale quantized command interrupted burst recording");
    p.quantize = false;
    // A changed sample rate resamples the stored phrase instead of discarding it.
    engine.prepare (24000, 256, 2); require (engine.restoreLoop (saved), "sample-rate restore rejected");
    run (256); require (engine.snapshotLoop().audio[0].size() == 12000, "loop was not resampled on restore");
    std::cout << "Performance DSP: beat-accurate looping, snapshots/restore, concurrent snapshots, resampling, burst, fades, history and allocation-free processing passed.\n";
}

std::vector<float> renderMaterial (motefield::EngineParameters p)
{
    motefield::Engine engine; engine.prepare (sampleRate, 256, 2);
    std::vector<float> result (96000), in (256), out (256), right (256), side (256);
    for (int offset = 0; offset < 96000; offset += 256)
    {
        const auto count = std::min (256, 96000 - offset);
        for (int i = 0; i < count; ++i) { in[i] = .3f * std::sin (twoPi * (offset + i) * 220.f / 48000.f); side[i] = ((offset + i) % 12000) < 1000 ? .8f : 0.f; }
        const float* inputs[] { in.data(), in.data() }; float* outputs[] { out.data(), right.data() };
        p.sidechain = side.data(); p.hostPositionValid = p.hostPlaying = true; p.hostPpq = offset / 24000.0;
        engine.process (inputs, outputs, 2, count, p); std::copy_n (out.begin(), count, result.begin() + offset);
    }
    return result;
}
void testMaterialAndPatterns()
{
    motefield::EngineParameters p; p.mode = motefield::Mode::veil; p.mix = 1; p.space = 0; p.patternLock = true;
    const auto base = renderMaterial (p);
    require (base == renderMaterial (p), "seeded pattern does not repeat across renders");
    const auto differs = [&] (const motefield::EngineParameters& changed, const char* message)
    { const auto next = renderMaterial (changed); double distance = 0; for (std::size_t i = 0; i < base.size(); ++i) { require (std::isfinite (next[i]), "material produced non-finite audio"); distance += std::abs (base[i] - next[i]); } require (distance > 1.0, message); };
    auto next = p; next.fieldPitch = 7; differs (next, "field pitch is cosmetic");
    next = p; next.fieldPosition = .5f; differs (next, "field position is cosmetic");
    next = p; next.fieldStretch = 2; differs (next, "stretch is cosmetic");
    next = p; next.fieldSplit = 1; differs (next, "split does not add voices");
    next = p; next.tension = 1; differs (next, "tension does not affect articulation");
    next = p; next.cohesion = 0; differs (next, "cohesion does not affect sound");
    next = p; next.fieldPitch = 7; next.viscosity = 1; const auto slow = renderMaterial (next); next.viscosity = 0; require (slow != renderMaterial (next), "viscosity does not change response time");
    next = p; next.magnetAmount = 1; differs (next, "magnet sidechain does not compress audio");
    next = p; next.rhythmMutation = 3; differs (next, "rhythm mutation has no effect");
    next = p; next.pitchMutation = 3; differs (next, "pitch mutation has no effect");
    next = p; next.scale = 2; differs (next, "scale does not constrain pitch");
    next = p; next.looperOnly = true; differs (next, "looper-only path still granular");
    std::cout << "Material DSP: audible gestures, viscosity/cohesion/tension, sidechain compression, reproducible patterns, independent mutations and scale constraints passed.\n";
}

} // namespace

int main (int argc, char**)
{
    testPerformanceFeatures();
    testMaterialAndPatterns();
    if (argc > 1) return 0;
    testAllModesAndVariations();
    testBlockSizeDeterminism();
    testFreeze();
    testPhraseLooper();
    testOverdubAccumulationAndUndo();
    testDelayHoldAndTelemetry();
    testOversizedBuffersAndBypass();
    testOutputWaveformTelemetry();
    testSpectralTelemetry();
    std::cout << "MoteField DSP tests passed: 44 programs, deterministic/oversized buffers, granular and delay Hold, layered overdub/undo, telemetry, mono bypass.\n";
    return 0;
}
