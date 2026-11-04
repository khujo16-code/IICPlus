#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <vector>

// Experimental real-time mono pitch shifter for the IIC+ input path.
// Uses two crossfaded delay taps. It is polyphonic in the sense that it shifts
// the composite waveform, but may produce modulation/comb artifacts. Listening
// tests are required before treating it as production-quality.
class PolyphonicTranspose final
{
public:
    static constexpr int minimumSemitones = -12;
    static constexpr int maximumSemitones = 12;

    void prepare(double newSampleRate) 
    {
        sampleRate = std::max(8000.0, newSampleRate);
        windowSamples = sampleRate * 0.040;
        const auto capacity = static_cast<std::size_t>(std::ceil(sampleRate * 0.080)) + 16;
        delayLine.assign(capacity, 0.0f); // allocated only during device preparation
        writeIndex = 0;
        phase = 0.0;
        currentRatio = 1.0;
        lastRequestedSemitones = 0;
        targetRatio = 1.0;
        prepared = true;
    }

    void reset() noexcept
    {
        std::fill(delayLine.begin(), delayLine.end(), 0.0f);
        writeIndex = 0;
        phase = 0.0;
        currentRatio = 1.0;
        lastRequestedSemitones = targetSemitones.load(std::memory_order_relaxed);
        targetRatio = ratioFor(lastRequestedSemitones);
    }

    void setSemitones(int value) noexcept
    {
        targetSemitones.store(std::clamp(value, minimumSemitones, maximumSemitones),
                              std::memory_order_relaxed);
    }

    int getSemitones() const noexcept
    {
        return targetSemitones.load(std::memory_order_relaxed);
    }

    bool isPrepared() const noexcept { return prepared; }

    // Called once per input sample on the audio thread. No allocation, locks,
    // or trigonometric calls occur per sample except the window crossfade.
    float processSample(float input) noexcept
    {
        if (!prepared || delayLine.empty())
            return input;

        delayLine[writeIndex] = input;
        const int requested = targetSemitones.load(std::memory_order_relaxed);
        if (requested != lastRequestedSemitones)
        {
            lastRequestedSemitones = requested;
            targetRatio = ratioFor(requested);
        }

        // Smooth changes to avoid clicks when the user changes semitones.
        currentRatio += (targetRatio - currentRatio) * 0.00025;

        float output = input; // exact transparent bypass at 0 semitones
        if (requested != 0 || std::abs(currentRatio - 1.0) > 0.00001)
        {
            const double phaseRate = (1.0 - currentRatio) / windowSamples;
            phase += phaseRate;
            phase -= std::floor(phase);

            const double phaseB = phase < 0.5 ? phase + 0.5 : phase - 0.5;
            const float gainA = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * pi * phase));
            const float gainB = 1.0f - gainA;
            const double delayA = 8.0 + phase * windowSamples;
            const double delayB = 8.0 + phaseB * windowSamples;
            output = readInterpolated(delayA) * gainA + readInterpolated(delayB) * gainB;
        }

        if (++writeIndex >= delayLine.size())
            writeIndex = 0;
        return output;
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    static double ratioFor(int semitones) noexcept
    {
        return std::pow(2.0, static_cast<double>(semitones) / 12.0);
    }

    float readInterpolated(double delay) const noexcept
    {
        const auto capacity = static_cast<long long>(delayLine.size());
        double position = static_cast<double>(writeIndex) - delay;
        while (position < 0.0)
            position += static_cast<double>(capacity);
        const double whole = std::floor(position);
        const auto index0 = static_cast<std::size_t>(static_cast<long long>(whole) % capacity);
        const auto index1 = (index0 + 1) % delayLine.size();
        const float fraction = static_cast<float>(position - whole);
        return delayLine[index0] + (delayLine[index1] - delayLine[index0]) * fraction;
    }

    std::vector<float> delayLine;
    std::atomic<int> targetSemitones { 0 };
    double sampleRate { 48000.0 };
    double windowSamples { 1920.0 };
    double phase { 0.0 };
    double currentRatio { 1.0 };
    double targetRatio { 1.0 };
    std::size_t writeIndex { 0 };
    int lastRequestedSemitones { 0 };
    bool prepared { false };
};
