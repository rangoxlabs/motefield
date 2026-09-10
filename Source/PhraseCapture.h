#pragma once

#include "DSP.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace motefield
{
// Audio owns writes and voice references. Snapshot/restore have one non-audio
// caller, serialized by the processor archive mutex. All samples are atomic so
// snapshots never race capture; bank destruction always stays off the audio thread.
class PhraseCapture
{
    static constexpr int slots = 12;
    struct Segment
    {
        std::array<std::unique_ptr<std::atomic<float>[]>, 2> audio;
        std::atomic<int> length { 0 };
        std::atomic<std::uint64_t> serial { 0 };
        int readers = 0;
    };
    struct Bank
    {
        explicit Bank (int size) : capacity (size)
        {
            for (auto& s : segments)
                for (auto& c : s.audio) c = std::make_unique<std::atomic<float>[]> (size);
        }
        std::array<Segment, slots> segments;
        int capacity;
    };
public:
    ~PhraseCapture() { dispose(); }
    void prepare (double rate)
    {
        dispose(); sampleRate = rate;
        envelopeCoefficient = static_cast<float> (1.0 - std::exp (-1.0 / (rate * .01)));
        peakDecay = static_cast<float> (std::exp (-1.0 / (rate * 2.0)));
        bank = new Bank (static_cast<int> (std::ceil (rate)));
        published.store (bank); reset();
    }
    void reset() noexcept
    {
        if (bank) for (auto& s : bank->segments) { s.length = 0; s.serial = 0; s.readers = 0; }
        current = -1; serial = 0; peak = envelope = performancePeak = 0.f;
        silent = sinceSignal = 0; waitingForAttack = matured = false; holdRequested = armFirstHold = false; locked.store (false);
    }
    bool beginBlock() noexcept
    {
        const auto write = retiredWrite.load (std::memory_order_relaxed);
        const auto nextWrite = (write + 1u) % retired.size();
        if (nextWrite == retiredRead.load (std::memory_order_acquire)) return false;
        auto* next = pending.exchange (nullptr);
        if (! next) return false;
        auto* previous = bank; bank = next; current = -1; serial = 0;
        matured = false;
        for (const auto& s : bank->segments)
        { serial = std::max (serial, s.serial.load()); matured = matured || s.length.load() >= static_cast<int> (sampleRate * .025); }
        published.store (bank); retired[write] = previous;
        retiredWrite.store (nextWrite, std::memory_order_release);
        silent = sinceSignal = 0; return true;
    }
    // Returns true when fresh usable material first becomes available.
    bool process (float l, float r, bool onset, bool hold) noexcept
    {
        if (! bank) return false;
        const float level = std::max (std::abs (l), std::abs (r));
        envelope += (level - envelope) * envelopeCoefficient;
        performancePeak = std::max (level, performancePeak * peakDecay);
        if (hold && ! holdRequested) armFirstHold = ! matured;
        holdRequested = hold;
        const bool retaining = hold && matured
            && (! armFirstHold || current < 0 || bank->segments[current].length.load() >= static_cast<int> (sampleRate * .2));
        locked.store (retaining, std::memory_order_relaxed);
        if (retaining) { current = -1; return false; }
        if (onset) waitingForAttack = false;
        if (level > .00002f) sinceSignal = 0; else ++sinceSignal;
        const int minimum = static_cast<int> (sampleRate * .025);
        if (current >= 0 && (onset && bank->segments[current].length.load() >= minimum))
            current = -1;
        if (current < 0 && ! waitingForAttack && envelope > .00006f)
        {
            int candidate = -1; auto oldest = std::numeric_limits<std::uint64_t>::max();
            for (int i = 0; i < slots; ++i)
                if (bank->segments[i].readers == 0 && bank->segments[i].serial.load() < oldest)
                { candidate = i; oldest = bank->segments[i].serial.load(); }
            if (candidate >= 0)
            {
                current = candidate; auto& s = bank->segments[current];
                s.serial.store (0); s.length.store (0); s.serial.store (++serial);
                peak = 0.f; silent = 0;
            }
        }
        if (current < 0) return false;
        auto& s = bank->segments[current]; const int length = s.length.load();
        peak = std::max (peak, level);
        silent = envelope < std::max (.00002f, peak * .018f) ? silent + 1 : 0;
        if (silent > static_cast<int> (sampleRate * .04) || length >= bank->capacity)
        { waitingForAttack = length < bank->capacity; current = -1; return false; }
        s.audio[0][length].store (l, std::memory_order_relaxed);
        s.audio[1][length].store (r, std::memory_order_relaxed);
        s.length.store (length + 1, std::memory_order_release);
        if (length + 1 == minimum) { matured = true; return true; }
        return false;
    }
    int select (int offset) const noexcept
    {
        std::array<int, slots> order {}; int count = 0;
        for (int i = 0; i < slots; ++i)
            if (bank->segments[i].length.load() >= static_cast<int> (sampleRate * .025))
                order[count++] = i;
        std::sort (order.begin(), order.begin() + count, [this] (int a, int b)
            { return bank->segments[a].serial.load() > bank->segments[b].serial.load(); });
        return count > 0 ? order[static_cast<unsigned> (offset) % static_cast<unsigned> (count)] : -1;
    }
    int length (int index) const noexcept { return bank->segments[index].length.load(); }
    std::uint64_t identity (int index) const noexcept { return bank->segments[index].serial.load(); }
    void retain (int index) noexcept { ++bank->segments[index].readers; }
    void release (int index) noexcept { if (index >= 0) --bank->segments[index].readers; }
    float read (int index, int channel, double position, int window) const noexcept
    {
        position = std::clamp (position, 0.0, static_cast<double> (window - 1));
        const auto i = static_cast<int> (position); const float fraction = static_cast<float> (position - i);
        const auto& samples = bank->segments[index].audio[channel];
        const float a = samples[i].load (std::memory_order_relaxed);
        return a + fraction * (samples[std::min (i + 1, window - 1)].load (std::memory_order_relaxed) - a);
    }
    float responseGain (float repeats, double quarter) const noexcept
    {
        if (locked.load()) return 1.f;
        return static_cast<float> (std::exp (-static_cast<double> (sinceSignal) /
            std::max (1.0, quarter * (1.0 + repeats * 8.0))));
    }
    bool isLocked() const noexcept { return locked.load(); }
    float participation() const noexcept
    {
        return locked.load() ? 1.f : 1.f - .22f * activity();
    }
    float activity() const noexcept { return std::clamp (envelope / std::max (.00008f, performancePeak), 0.f, 1.f); }
    PhraseSnapshot snapshot() const
    {
        PhraseSnapshot result; result.sampleRate = sampleRate;
        auto* queued = pending.load();
        if (! queued && ! locked.load()) return result;
        auto* source = queued ? queued : published.load();
        for (int i = 0; i < slots; ++i)
        {
            const auto& s = source->segments[i];
            const auto revision = s.serial.load(); const auto n = s.length.load();
            result.lengths[i] = n; result.identities[i] = revision;
            for (int c = 0; c < 2; ++c)
                for (int j = 0; j < n; ++j) result.audio[c].push_back (s.audio[c][j].load());
            if (s.serial.load() != revision) return {}; // Capture resumed during snapshot.
        }
        return result;
    }
    bool restore (const PhraseSnapshot& snapshot)
    {
        collectRetired();
        if (! std::isfinite (snapshot.sampleRate)
            || snapshot.sampleRate < 8000 || snapshot.sampleRate > 384000) return false;
        std::size_t total = 0;
        for (auto n : snapshot.lengths)
        { if (n < 0 || n > std::ceil (snapshot.sampleRate)) return false; total += static_cast<std::size_t> (n); }
        for (const auto& c : snapshot.audio)
        {
            if (c.size() != total) return false;
            for (float v : c) if (! std::isfinite (v)) return false;
        }
        auto next = std::make_unique<Bank> (static_cast<int> (std::ceil (sampleRate)));
        std::size_t offset = 0;
        for (int i = 0; i < slots; ++i)
        {
            const int oldN = snapshot.lengths[i];
            const int n = std::clamp (static_cast<int> (std::round (oldN * sampleRate / snapshot.sampleRate)), 0, next->capacity);
            auto& s = next->segments[i]; s.length = n; s.serial = snapshot.identities[i];
            for (int c = 0; c < 2; ++c)
                for (int j = 0; j < n && oldN > 0; ++j)
                {
                    const auto x = std::min (j * snapshot.sampleRate / sampleRate, static_cast<double> (oldN - 1));
                    const auto k = static_cast<int> (x); const float a = snapshot.audio[c][offset + k];
                    s.audio[c][j] = a + static_cast<float> (x - k) * (snapshot.audio[c][offset + std::min (k + 1, oldN - 1)] - a);
                }
            offset += oldN;
        }
        delete pending.exchange (next.release()); return true;
    }
private:
    void dispose()
    {
        delete pending.exchange (nullptr); collectRetired();
        delete bank; bank = nullptr; published.store (nullptr);
    }
    void collectRetired()
    {
        auto read = retiredRead.load (std::memory_order_relaxed);
        const auto write = retiredWrite.load (std::memory_order_acquire);
        while (read != write) { delete retired[read]; read = (read + 1u) % retired.size(); }
        retiredRead.store (read, std::memory_order_release);
    }
    Bank* bank = nullptr;
    std::atomic<Bank*> published { nullptr }, pending { nullptr };
    std::array<Bank*, 8> retired {};
    std::atomic<std::size_t> retiredRead { 0 }, retiredWrite { 0 };
    std::atomic<bool> locked { false };
    double sampleRate = 44100.;
    int current = -1, silent = 0;
    std::uint64_t serial = 0, sinceSignal = 0;
    float peak = 0.f, envelope = 0.f, performancePeak = 0.f;
    float envelopeCoefficient = .002f, peakDecay = .99999f;
    bool waitingForAttack = false;
    bool matured = false;
    bool holdRequested = false, armFirstHold = false;
};
}
