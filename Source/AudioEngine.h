#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <array>
#include <mutex>
#include <thread>

// NAM core's public loading API. NeuralAmpModelerCore exports this header from NAM/.
#include <get_dsp.h>
#include "DSP/PolyphonicTranspose.h"

class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void start();
    void stop();
    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    void setInputChannel(int channel) noexcept { inputChannel.store(juce::jlimit(0, 1, channel)); }
    juce::String getStatus() const;
    float getInputPeakLinear() const noexcept { return inputPeakLinear.load(); }
    float getOutputPeakLinear() const noexcept { return outputPeakLinear.load(); }
    float getTunerFrequencyHz() const noexcept { return tunerFrequencyHz.load(std::memory_order_relaxed); }
    void setTunerEnabled(bool enabled) noexcept;
    void setTransposeSemitones(int value) noexcept { transpose.setSemitones(value); }
    int getTransposeSemitones() const noexcept { return transpose.getSemitones(); }
    bool isTunerEnabled() const noexcept { return tunerEnabled.load(std::memory_order_relaxed); }
    float getCleanChannelMix() const noexcept { return cleanChannelMixForUi.load(); }
    bool consumeInputClip() noexcept { return inputClip.exchange(false); }
    bool consumeOutputClip() noexcept { return outputClip.exchange(false); }
    bool loadNamFile(const juce::File& file);
    bool loadCleanNamFile(const juce::File& file);
    bool loadIrFile(const juce::File& file);
    juce::File getNamFile() const { return namFile; }
    juce::File getCleanNamFile() const { return cleanNamFile; }
    juce::File getIrFile() const { return irFile; }

    void setVolume1(float value) noexcept {
        const float volume = juce::jlimit(0.0f, 10.0f, value);
        volume1.store(volume, std::memory_order_relaxed);

        // A gentle audio-style taper: keep the established endpoints and the
        // 5.0 midpoint unchanged, but make the middle of the knob less abrupt.
        // The NAM profile remains responsible for the actual amp saturation;
        // this only controls how strongly Volume 1 drives its input.
        constexpr float taper = 1.25f;
        const float normalized = volume <= 5.0f
            ? -1.0f + std::pow(volume / 5.0f, taper)
            : std::pow((volume - 5.0f) / 5.0f, taper);
        inputGainDb.store(24.0f * normalized, std::memory_order_relaxed);
    }
    void setLeadMode(bool enabled) noexcept { leadMode.store(enabled); }
    void setNoiseGateEnabled(bool enabled) noexcept { noiseGateEnabled.store(enabled, std::memory_order_relaxed); }
    void setNoiseGateThreshold(float value) noexcept { noiseGateThreshold.store(juce::jlimit(-80.0f, -20.0f, value), std::memory_order_relaxed); }
    void setTs9Enabled(bool enabled) noexcept { ts9Enabled.store(enabled, std::memory_order_relaxed); }
    void setTs9Drive(float value) noexcept { ts9Drive.store(juce::jlimit(0.0f, 10.0f, value), std::memory_order_relaxed); }
    void setTs9Tone(float value) noexcept { ts9Tone.store(juce::jlimit(0.0f, 10.0f, value), std::memory_order_relaxed); }
    void setTs9Level(float value) noexcept { ts9Level.store(juce::jlimit(0.0f, 10.0f, value), std::memory_order_relaxed); }
    void setCompressorEnabled(bool enabled) noexcept { compressorEnabled.store(enabled, std::memory_order_relaxed); }
    void setCompressorSustain(float value) noexcept { compressorSustain.store(juce::jlimit(0.0f, 10.0f, value), std::memory_order_relaxed); }
    void setDelayEnabled(bool enabled) noexcept { delayEnabled.store(enabled, std::memory_order_relaxed); }
    void setDelayTimeMs(float value) noexcept { delayTimeMs.store(juce::jlimit(50.0f, 1000.0f, value), std::memory_order_relaxed); }
    void setDelayFeedback(float value) noexcept { delayFeedback.store(juce::jlimit(0.0f, 0.85f, value), std::memory_order_relaxed); }
    void setDelayMix(float value) noexcept { delayMix.store(juce::jlimit(0.0f, 0.5f, value), std::memory_order_relaxed); }
    void setChorusEnabled(bool enabled) noexcept { chorusEnabled.store(enabled, std::memory_order_relaxed); }
    void setChorusRate(float value) noexcept { chorusRate.store(juce::jlimit(0.1f, 5.0f, value), std::memory_order_relaxed); }
    void setChorusDepth(float value) noexcept { chorusDepth.store(juce::jlimit(0.0f, 10.0f, value), std::memory_order_relaxed); }
    void setChorusMix(float value) noexcept { chorusMix.store(juce::jlimit(0.0f, 0.5f, value), std::memory_order_relaxed); }
    void setReverbEnabled(bool enabled) noexcept { reverbEnabled.store(enabled, std::memory_order_relaxed); }
    void setReverbDecay(float value) noexcept { reverbDecay.store(juce::jlimit(0.3f, 8.0f, value), std::memory_order_relaxed); }
    void setReverbPreDelay(float value) noexcept { reverbPreDelay.store(juce::jlimit(0.0f, 120.0f, value), std::memory_order_relaxed); }
    void setReverbSize(float value) noexcept { reverbSize.store(juce::jlimit(0.0f, 10.0f, value), std::memory_order_relaxed); }
    void setReverbMix(float value) noexcept { reverbMix.store(juce::jlimit(0.0f, 100.0f, value), std::memory_order_relaxed); }
    void setReverbType(int value) noexcept { reverbType.store(juce::jlimit(0, 3, value), std::memory_order_relaxed); }
    void setOutputGainDb(float value) noexcept { outputGainDb.store(value); }
    void setLeadMasterDb(float value) noexcept { leadMasterDb.store(value); }
    void setEqGainDb(int band, float value) noexcept;
    void setBright(bool enabled) noexcept { bright.store(enabled); }
    void setDeep(bool enabled) noexcept { deep.store(enabled); }
    void setTrebleShift(bool enabled) noexcept { trebleShift.store(enabled); }
    void setBassShift(bool enabled) noexcept { bassShift.store(enabled); }
    void setToneControl(int control, float valueDb) noexcept;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
        int numInputChannels, float* const* outputChannelData, int numOutputChannels,
        int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    void prepare(double sampleRate, int blockSize);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void updateFilters();
    void tunerWorkerLoop();
    float analyseTunerWindow(std::array<float, 4096>& window, double sampleRate) const noexcept;

    juce::AudioDeviceManager deviceManager;
    PolyphonicTranspose transpose;
    std::atomic<float> tunerFrequencyHz { 0.0f };
    // Lock-free single-producer/single-consumer sample queue. The audio callback
    // only copies samples; the YIN calculation runs on tunerWorker.
    static constexpr int tunerWindowSize = 4096;
    static constexpr uint32_t tunerQueueSize = 16384;
    std::array<float, tunerQueueSize> tunerQueue {};
    std::atomic<uint32_t> tunerWriteCounter { 0 }, tunerReadCounter { 0 };
    std::atomic<bool> tunerEnabled { true }, tunerWorkerRunning { false };
    std::atomic<double> tunerSampleRate { 48000.0 };
    std::thread tunerWorker;
    juce::String status { "Choose an audio device, then load a NAM profile and IR." };
    juce::File namFile, cleanNamFile, irFile;
    mutable juce::CriticalSection statusLock;

    // Model replacement happens off the audio thread; access is guarded at block boundaries.
    std::mutex modelMutex;
    std::unique_ptr<nam::DSP> namModel;
    std::unique_ptr<nam::DSP> cleanNamModel;
    std::unique_ptr<nam::DSP> irModel;
    juce::AudioBuffer<float> monoInput, monoOutput, irOutput;
    std::vector<float> inputScratch, outputScratch, cleanOutputScratch, irScratch;
    std::vector<float*> namInPtrs, namOutPtrs, cleanNamOutPtrs, irInPtrs, irOutPtrs;

    juce::dsp::NoiseGate<float> noiseGate;
    juce::dsp::Compressor<float> guitarCompressor;
    juce::dsp::IIR::Filter<float> eq80, eq240, eq750, eq2200, eq6600;
    juce::dsp::IIR::Filter<float> brightFilter, deepFilter, trebleShiftFilter, trebleShiftHighCutFilter, bassShiftFilter;
    juce::dsp::IIR::Filter<float> bassFilter, midFilter, trebleFilter, presenceFilter;
    juce::dsp::ProcessSpec spec {};
    std::array<std::atomic<float>, 5> eqDb;
    std::array<std::atomic<float>, 4> toneDb; // bass, mid, treble, presence; post-NAM approximations
    std::atomic<float> noiseGateThreshold { -55.0f };
    std::atomic<float> compressorSustain { 3.0f };
    std::atomic<bool> compressorEnabled { false };
    std::atomic<bool> delayEnabled { false };
    std::atomic<float> delayTimeMs { 350.0f }, delayFeedback { 0.32f }, delayMix { 0.22f };
    std::vector<float> delayBuffer;
    size_t delayWriteIndex { 0 };
    float delayLowpassState { 0.0f };
    float delayTimeSamplesCurrent { 16800.0f };
    std::atomic<bool> chorusEnabled { false };
    std::atomic<float> chorusRate { 0.8f }, chorusDepth { 5.0f }, chorusMix { 0.22f };
    std::array<std::vector<float>, 2> chorusBuffers;
    size_t chorusWriteIndex { 0 };
    double chorusPhase { 0.0 };
    float chorusEnableMixCurrent { 0.0f }, chorusRateCurrent { 0.8f }, chorusDepthCurrent { 5.0f }, chorusMixCurrent { 0.22f };
    std::atomic<bool> reverbEnabled { false };
    std::atomic<float> reverbDecay { 2.2f }, reverbPreDelay { 18.0f }, reverbSize { 5.0f }, reverbMix { 20.0f };
    std::atomic<int> reverbType { 1 }; // Room, Studio, Plate, Hall
    juce::Reverb reverb;
    std::array<std::vector<float>, 2> reverbPreDelayBuffers;
    std::array<std::vector<float>, 2> reverbScratch, reverbDryScratch;
    size_t reverbPreDelayWriteIndex { 0 };
    float reverbPreDelaySamplesCurrent { 0.0f }, reverbMixCurrent { 0.0f };
    std::atomic<float> ts9Drive { 2.0f }, ts9Tone { 5.0f }, ts9Level { 5.0f };
    std::atomic<bool> ts9Enabled { false };
    float ts9HpState { 0.0f }, ts9ToneState { 0.0f };
    std::atomic<float> inputGainDb { 0.0f }, volume1 { 5.0f }, leadMasterDb { 0.0f }, outputGainDb { -6.0f };
    std::atomic<float> inputPeakLinear { 0.0f }, outputPeakLinear { 0.0f };
    std::atomic<float> cleanChannelMixForUi { 0.0f };
    std::atomic<bool> inputClip { false }, outputClip { false };
    std::atomic<int> inputChannel { 1 }; // Scarlett guitar jack is normally channel 2 (zero-based index 1).
    std::atomic<bool> noiseGateEnabled { false };
    std::atomic<bool> bright { false }, deep { false }, trebleShift { false }, bassShift { false }, cleanNamLoaded { false };
    std::atomic<bool> leadMode { true }; // Pull Lead ON selects lead; OFF selects clean/rhythm.
    // Audio-thread-only smoothed switch mix values prevent clicks/pops on toggles.
    float brightMixCurrent { 0.0f }, deepMixCurrent { 0.0f }, trebleShiftMixCurrent { 0.0f }, bassShiftMixCurrent { 0.0f }, cleanMixCurrent { 0.0f };
    
    double currentSampleRate { 48000.0 };
    int currentBlockSize { 512 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
