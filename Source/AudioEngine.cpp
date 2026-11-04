#include "AudioEngine.h"
#include <cmath>
#include <chrono>

AudioEngine::AudioEngine()
{
    for (auto& value : eqDb) value.store(0.0f);
    for (auto& value : toneDb) value.store(0.0f);
    const auto settingsFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                    .getChildFile("IICPlusNAM");
    const auto savedAudioState = settingsFolder.getChildFile("audio-device.xml");
    std::unique_ptr<juce::XmlElement> savedState;
    if (savedAudioState.existsAsFile())
        savedState = juce::XmlDocument::parse(savedAudioState);
    deviceManager.initialise(2, 2, savedState.get(), true);
    tunerWorkerRunning.store(true, std::memory_order_release);
    tunerWorker = std::thread(&AudioEngine::tunerWorkerLoop, this);
    deviceManager.addAudioCallback(this);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    tunerWorkerRunning.store(false, std::memory_order_release);
    if (tunerWorker.joinable()) tunerWorker.join();
}

void AudioEngine::start()
{
    // AudioDeviceManager starts the selected device when one is available.
    const auto* device = deviceManager.getCurrentAudioDevice();
    juce::ScopedLock lock(statusLock);
    status = device ? ("Audio device: " + device->getName()) : "No audio device selected.";
}

void AudioEngine::setTunerEnabled(bool enabled) noexcept
{
    tunerEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) tunerFrequencyHz.store(0.0f, std::memory_order_relaxed);
}

void AudioEngine::stop()
{
    deviceManager.closeAudioDevice();
}

juce::String AudioEngine::getStatus() const
{
    juce::ScopedLock lock(statusLock);
    return status;
}

bool AudioEngine::loadNamFile(const juce::File& file)
{
    if (!file.existsAsFile() || file.getFileExtension().compareIgnoreCase(".nam") != 0)
        return false;

    try
    {
        auto candidate = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
        candidate->Reset(currentSampleRate, currentBlockSize);
        candidate->prewarm(); // Never call from the real-time audio callback.
        {
            std::lock_guard<std::mutex> lock(modelMutex);
            namModel = std::move(candidate);
        }
        namFile = file;
        juce::ScopedLock lock(statusLock);
        status = "Loaded NAM: " + file.getFileName();
        return true;
    }
    catch (const std::exception& e)
    {
        juce::ScopedLock lock(statusLock);
        status = "NAM load failed: " + juce::String(e.what());
        return false;
    }
}

bool AudioEngine::loadCleanNamFile(const juce::File& file)
{
    if (!file.existsAsFile() || file.getFileExtension().compareIgnoreCase(".nam") != 0)
        return false;

    try
    {
        auto candidate = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
        candidate->Reset(currentSampleRate, currentBlockSize);
        candidate->prewarm();
        {
            std::lock_guard<std::mutex> lock(modelMutex);
            cleanNamModel = std::move(candidate);
        }
        cleanNamFile = file;
        cleanNamLoaded.store(true);
        juce::ScopedLock lock(statusLock);
        status = "Loaded clean NAM: " + file.getFileName();
        return true;
    }
    catch (const std::exception& e)
    {
        juce::ScopedLock lock(statusLock);
        status = "Clean NAM load failed: " + juce::String(e.what());
        return false;
    }
}

bool AudioEngine::loadIrFile(const juce::File& file)
{
    if (!file.existsAsFile() || file.getFileExtension().compareIgnoreCase(".wav") != 0)
        return false;

    // This starter's IR output buffers are mono. Reject stereo files until stereo routing
    // is implemented, rather than passing too few output pointers to the NAM DSP.
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->numChannels != 1)
    {
        juce::ScopedLock lock(statusLock);
        status = "Please use a mono WAV IR (stereo IR support is planned).";
        return false;
    }

    try
    {
        // NeuralAmpModelerCore can load WAV files as linear convolution models.
        auto candidate = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
        candidate->Reset(currentSampleRate, currentBlockSize);
        {
            std::lock_guard<std::mutex> lock(modelMutex);
            irModel = std::move(candidate);
        }
        irFile = file;
        juce::ScopedLock lock(statusLock);
        status = "Loaded IR: " + file.getFileName();
        return true;
    }
    catch (const std::exception& e)
    {
        juce::ScopedLock lock(statusLock);
        status = "IR load failed: " + juce::String(e.what());
        return false;
    }
}

void AudioEngine::setEqGainDb(int band, float value) noexcept
{
    if (band >= 0 && band < static_cast<int>(eqDb.size()))
        eqDb[static_cast<size_t>(band)].store(juce::jlimit(-12.0f, 12.0f, value));
}

void AudioEngine::setToneControl(int control, float valueDb) noexcept
{
    if (control >= 0 && control < static_cast<int>(toneDb.size()))
        toneDb[static_cast<size_t>(control)].store(juce::jlimit(-6.0f, 6.0f, valueDb));
}

void AudioEngine::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    currentBlockSize = juce::jmax(16, blockSize);
    spec = { currentSampleRate, static_cast<juce::uint32>(currentBlockSize), 1 };
    transpose.prepare(currentSampleRate);
    monoInput.setSize(1, currentBlockSize);
    monoOutput.setSize(2, currentBlockSize);
    irOutput.setSize(2, currentBlockSize);
    inputScratch.resize(static_cast<size_t>(currentBlockSize));
    outputScratch.resize(static_cast<size_t>(currentBlockSize));
    cleanOutputScratch.resize(static_cast<size_t>(currentBlockSize));
    irScratch.resize(static_cast<size_t>(currentBlockSize));
    delayBuffer.assign(static_cast<size_t>(currentSampleRate * 1.1) + 4, 0.0f);
    delayWriteIndex = 0;
    delayLowpassState = 0.0f;
    delayTimeSamplesCurrent = currentSampleRate * 0.350f;
    const auto chorusBufferSize = static_cast<size_t>(currentSampleRate * 0.060) + 4;
    for (auto& channel : chorusBuffers) channel.assign(chorusBufferSize, 0.0f);
    chorusWriteIndex = 0;
    chorusPhase = 0.0;
    chorusEnableMixCurrent = chorusEnabled.load() ? 1.0f : 0.0f;
    chorusRateCurrent = chorusRate.load();
    chorusDepthCurrent = chorusDepth.load();
    chorusMixCurrent = chorusMix.load();
    const auto reverbBufferSize = static_cast<size_t>(currentSampleRate * 0.260) + 4;
    for (auto& channel : reverbPreDelayBuffers) channel.assign(reverbBufferSize, 0.0f);
    for (auto& channel : reverbScratch) channel.assign(static_cast<size_t>(currentBlockSize), 0.0f);
    for (auto& channel : reverbDryScratch) channel.assign(static_cast<size_t>(currentBlockSize), 0.0f);
    reverbPreDelayWriteIndex = 0;
    reverbPreDelaySamplesCurrent = reverbPreDelay.load() * 0.001f * (float)currentSampleRate;
    reverbMixCurrent = 0.0f;
    reverb.setSampleRate(currentSampleRate);
    reverb.reset();
    namInPtrs = { inputScratch.data() };
    namOutPtrs = { outputScratch.data() };
    cleanNamOutPtrs = { cleanOutputScratch.data() };
    irInPtrs = { outputScratch.data() };
    irOutPtrs = { irScratch.data() };
    updateFilters();
    noiseGate.prepare(spec);
    noiseGate.reset();
    guitarCompressor.prepare(spec);
    guitarCompressor.reset();
    ts9HpState = ts9ToneState = 0.0f;
    bassShiftFilter.reset();

    std::lock_guard<std::mutex> lock(modelMutex);
    if (namModel) namModel->Reset(currentSampleRate, currentBlockSize);
    if (cleanNamModel) cleanNamModel->Reset(currentSampleRate, currentBlockSize);
    if (irModel) irModel->Reset(currentSampleRate, currentBlockSize);
}

void AudioEngine::updateFilters()
{
    auto peak = [this](float frequency, float gainDb)
    {
        return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            currentSampleRate, frequency, 0.85f, juce::Decibels::decibelsToGain(gainDb));
    };
    eq80.coefficients = peak(80.0f, eqDb[0].load());
    eq240.coefficients = peak(240.0f, eqDb[1].load());
    eq750.coefficients = peak(750.0f, eqDb[2].load());
    eq2200.coefficients = peak(2200.0f, eqDb[3].load());
    eq6600.coefficients = peak(6600.0f, eqDb[4].load());
    // Keep the voicing filters running continuously and crossfade their contribution
    // in the sample loop. This avoids abrupt coefficient changes when a switch toggles.
    // Volume 1 Pull Bright: strongest at low Volume 1 settings and progressively
    // reduced as Volume 1 rises, reaching zero at 10. This is a post-NAM
    // approximation of the original control interaction.
    const float brightGainDb = 5.0f * (1.0f - juce::jlimit(0.0f, 10.0f, volume1.load()) / 10.0f);
    brightFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 850.0f, 0.707f, juce::Decibels::decibelsToGain(brightGainDb));
    deepFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        currentSampleRate, 110.0f, 0.707f, juce::Decibels::decibelsToGain(3.0f));
    // Treble Pull Shift approximation: shift the perceived emphasis toward the
    // lower mids and gently tame the upper treble, rather than adding a high shelf.
    // This is a post-NAM approximation, applied only in lead mode below.
    trebleShiftFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, 900.0f, 0.70f, juce::Decibels::decibelsToGain(3.0f));
    trebleShiftHighCutFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 3600.0f, 0.707f, juce::Decibels::decibelsToGain(-1.5f));
    // Bass Pull Shift approximation: a restrained sub-low shelf placed BEFORE
    // the NAM capture so the added low frequencies feed the captured gain stages.
    // This better reflects the original early-in-the-preamp placement. Start at
    // +2 dB around 60 Hz; the real switch is intended mainly for lower-gain sounds.
    bassShiftFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        currentSampleRate, 60.0f, 0.707f, juce::Decibels::decibelsToGain(2.0f));
    // Post-NAM approximation of the Mark IIC+ tone-stack interaction.
    // Treble is first in the original tone-control string: at higher Treble settings
    // it increasingly dominates and reduces how strongly Bass and Middle shape the
    // result. We model that with a gentle, smooth reduction of the Bass/Middle filter
    // ranges above the Treble midpoint. Lower Treble settings leave Bass/Middle fully
    // effective, so their relative contribution naturally becomes more prominent.
    // This is deliberately a post-capture tonal approximation; it cannot recreate
    // the original pre-gain change in drive/saturation.
    const float bass = toneDb[0].load();
    const float mid = toneDb[1].load();
    const float treble = toneDb[2].load();
    const float presence = toneDb[3].load();
    const float treblePosition = juce::jlimit(0.0f, 1.0f, (treble + 6.0f) / 12.0f);
    const float trebleDominance = juce::jmax(0.0f, (treblePosition - 0.5f) * 2.0f);
    const float bassMidInfluence = 1.0f - 0.25f * trebleDominance;

    // Higher Volume 1 settings in the real IIC+ call for progressively less
    // preamp Bass to keep the attack focused. The NAM capture already supplies
    // the nonlinear gain response, so this post-NAM approximation gently
    // reduces the *Bass control's contribution* above 6 rather than adding a
    // second gain stage or indiscriminately cutting the entire low end.
    const float volume1Position = juce::jlimit(0.0f, 10.0f, volume1.load());
    const float highGainBassReduction = juce::jmap(
        juce::jlimit(0.0f, 1.0f, (volume1Position - 6.0f) / 4.0f),
        1.0f, 0.70f);
    const float volumeAdjustedBass = bass * highGainBassReduction;

    const float effectiveBass = juce::jlimit(-6.0f, 6.0f,
        volumeAdjustedBass * bassMidInfluence - 0.12f * mid - 0.10f * treble);
    // Middle control response: retain the broad 750 Hz character, but give
    // the useful middle of the knob a little more resolution and authority.
    // This gentle S-shaped taper preserves the exact centre (0 dB) and both
    // endpoints, while avoiding an abrupt or exaggerated mid boost/cut.
    const float normalizedMid = juce::jlimit(-1.0f, 1.0f, mid / 6.0f);
    const float shapedMid = 6.0f * (normalizedMid
        + 0.12f * normalizedMid * (1.0f - normalizedMid * normalizedMid));
    const float effectiveMid = juce::jlimit(-6.0f, 6.0f,
        shapedMid * bassMidInfluence - 0.10f * bass - 0.16f * treble);
    const float effectiveTreble = juce::jlimit(-6.0f, 6.0f,
        treble - 0.10f * bass + 0.08f * mid);
    bassFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        currentSampleRate, 130.0f, 0.707f, juce::Decibels::decibelsToGain(effectiveBass));
    midFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, 750.0f, 0.8f, juce::Decibels::decibelsToGain(effectiveMid));
    trebleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 2200.0f, 0.707f, juce::Decibels::decibelsToGain(effectiveTreble));
    presenceFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 4800.0f, 0.707f, juce::Decibels::decibelsToGain(presence));
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
        tunerSampleRate.store(device->getCurrentSampleRate(), std::memory_order_relaxed);
    }
}

void AudioEngine::audioDeviceStopped() {}

float AudioEngine::analyseTunerWindow(std::array<float, tunerWindowSize>& window, double sampleRate) const noexcept
{
    float mean = 0.0f;
    for (float sample : window) mean += sample;
    mean /= (float)tunerWindowSize;
    float energy = 0.0f;
    for (auto& sample : window) { sample -= mean; energy += sample * sample; }
    if (energy / (float)tunerWindowSize <= 1.0e-5f || sampleRate <= 0.0) return 0.0f;

    constexpr int minLag = 20, maxLag = 1200;
    std::array<float, maxLag + 1> cmnd {};
    float runningSum = 0.0f;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        float sum = 0.0f;
        for (int i = 0; i < tunerWindowSize - tau; ++i)
        { const float delta = window[(size_t)i] - window[(size_t)(i + tau)]; sum += delta * delta; }
        runningSum += sum;
        cmnd[(size_t)tau] = runningSum > 0.0f ? sum * (float)tau / runningSum : 1.0f;
    }
    int bestLag = -1;
    for (int tau = minLag; tau < maxLag; ++tau)
        if (cmnd[(size_t)tau] < 0.15f) { while (tau + 1 < maxLag && cmnd[(size_t)(tau + 1)] < cmnd[(size_t)tau]) ++tau; bestLag = tau; break; }
    if (bestLag < 0)
    {
        float bestValue = 0.30f;
        for (int tau = minLag; tau <= maxLag; ++tau)
            if (cmnd[(size_t)tau] < bestValue) { bestValue = cmnd[(size_t)tau]; bestLag = tau; }
    }
    if (bestLag <= 0) return 0.0f;
    const float left = cmnd[(size_t)juce::jmax(minLag, bestLag - 1)];
    const float centre = cmnd[(size_t)bestLag];
    const float right = cmnd[(size_t)juce::jmin(maxLag, bestLag + 1)];
    const float denominator = left - 2.0f * centre + right;
    const float correction = std::abs(denominator) > 1.0e-9f ? 0.5f * (left - right) / denominator : 0.0f;
    const float period = (float)bestLag + juce::jlimit(-0.5f, 0.5f, correction);
    const float hz = (float)sampleRate / period;
    return (hz >= 40.0f && hz <= 1200.0f) ? hz : 0.0f;
}

void AudioEngine::tunerWorkerLoop()
{
    std::array<float, tunerWindowSize> history {};
    std::array<float, tunerWindowSize> window {};
    int writePosition = 0, samplesSinceAnalysis = 0;
    while (tunerWorkerRunning.load(std::memory_order_acquire))
    {
        if (!tunerEnabled.load(std::memory_order_relaxed))
        {
            tunerReadCounter.store(tunerWriteCounter.load(std::memory_order_acquire), std::memory_order_release);
            writePosition = samplesSinceAnalysis = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        auto read = tunerReadCounter.load(std::memory_order_relaxed);
        const auto write = tunerWriteCounter.load(std::memory_order_acquire);
        if (read == write) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
        while (read != write)
        {
            history[(size_t)writePosition] = tunerQueue[read % tunerQueueSize];
            writePosition = (writePosition + 1) % tunerWindowSize;
            ++read; ++samplesSinceAnalysis;
            if (samplesSinceAnalysis >= tunerWindowSize / 2)
            {
                samplesSinceAnalysis = 0;
                for (int i = 0; i < tunerWindowSize; ++i)
                    window[(size_t)i] = history[(size_t)((writePosition + i) % tunerWindowSize)];
                const float hz = analyseTunerWindow(window, tunerSampleRate.load(std::memory_order_relaxed));
                if (tunerEnabled.load(std::memory_order_relaxed))
                    tunerFrequencyHz.store(hz, std::memory_order_relaxed);
            }
        }
        tunerReadCounter.store(read, std::memory_order_release);
    }
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
    int numInputChannels, float* const* outputChannelData, int numOutputChannels,
    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    if (numInputChannels < 1 || inputChannelData == nullptr)
        return;

    // Prefer the selected input channel (default: channel 2 for a Scarlett guitar input).
    // Fall back to channel 1 if the active device exposes only one input.
    const int requestedChannel = inputChannel.load();
    const int sourceChannel = (requestedChannel < numInputChannels
        && inputChannelData[requestedChannel] != nullptr) ? requestedChannel : 0;
    if (inputChannelData[sourceChannel] == nullptr)
        return;

    // Real-time-safe tuner feed: no pitch analysis, allocation, or locking here.
    if (tunerEnabled.load(std::memory_order_relaxed))
    {
        auto write = tunerWriteCounter.load(std::memory_order_relaxed);
        const auto read = tunerReadCounter.load(std::memory_order_acquire);
        const auto available = tunerQueueSize - (write - read);
        const auto count = juce::jmin<uint32_t>((uint32_t)numSamples, available);
        for (uint32_t i = 0; i < count; ++i)
            tunerQueue[(write + i) % tunerQueueSize] = inputChannelData[sourceChannel][i];
        tunerWriteCounter.store(write + count, std::memory_order_release);
    }

    // JUCE may deliver a block larger than the last reported size. Process it in slices.
    int offset = 0;
    float blockInputPeak = 0.0f;
    float blockOutputPeak = 0.0f;
    bool blockInputClip = false;
    bool blockOutputClip = false;
    while (offset < numSamples)
    {
        const int n = juce::jmin(currentBlockSize, numSamples - offset);
        const bool gateActive = noiseGateEnabled.load(std::memory_order_relaxed);
        if (gateActive)
        {
            noiseGate.setThreshold(noiseGateThreshold.load(std::memory_order_relaxed));
            noiseGate.setRatio(12.0f);
            // Fixed all-round guitar settings: fast enough to preserve pick attack,
            // with a natural release that lets sustained notes decay smoothly.
            noiseGate.setAttack(2.0f);
            noiseGate.setRelease(140.0f);
        }
        const bool compressorActive = compressorEnabled.load(std::memory_order_relaxed);
        if (compressorActive)
        {
            const float sustain = compressorSustain.load(std::memory_order_relaxed);
            guitarCompressor.setThreshold(-12.0f - sustain * 2.2f);
            guitarCompressor.setRatio(1.4f + sustain * 0.28f);
            guitarCompressor.setAttack(12.0f);
            guitarCompressor.setRelease(120.0f);
        }
        for (int i = 0; i < n; ++i)
        {
            const float rawInput = inputChannelData[sourceChannel][offset + i];
            const float inputAbs = std::abs(rawInput);
            blockInputPeak = juce::jmax(blockInputPeak, inputAbs);
            blockInputClip = blockInputClip || inputAbs >= 0.999f;
            // Tuner has already received rawInput above. Transpose therefore
            // changes the audible signal without changing the tuner reading.
            const float transposedInput = transpose.processSample(rawInput);
            float processedInput = transposedInput
                * juce::Decibels::decibelsToGain(inputGainDb.load());

            // PRE noise gate: placed after the guitar input gain and before
            // Bass Shift / NAM, so it removes idle pickup/interface noise
            // without changing the approved amp signal path when bypassed.
            if (gateActive)
                processedInput = noiseGate.processSample(0, processedInput);

            // TS9-style overdrive: pre-NAM, with a focused midrange input filter,
            // smooth asymmetric soft clipping, and independent pedal output level.
            if (ts9Enabled.load(std::memory_order_relaxed))
            {
                const float drive = ts9Drive.load(std::memory_order_relaxed);
                const float tone = ts9Tone.load(std::memory_order_relaxed);
                const float level = ts9Level.load(std::memory_order_relaxed);
                const float hpHz = 260.0f + 420.0f * (drive / 10.0f);
                const float hpA = std::exp(-2.0f * juce::MathConstants<float>::pi * hpHz / (float)currentSampleRate);
                ts9HpState = hpA * ts9HpState + (1.0f - hpA) * processedInput;
                const float focused = processedInput - ts9HpState;
                const float preGain = 1.0f + drive * 1.15f;
                const float x = focused * preGain;
                // Slightly asymmetric transfer gives a softer, pedal-like clipping character.
                const float clipped = x >= 0.0f ? std::tanh(x * 1.12f) : std::tanh(x * 0.96f);
                const float toneHz = 1800.0f + tone * 520.0f;
                const float toneA = std::exp(-2.0f * juce::MathConstants<float>::pi * toneHz / (float)currentSampleRate);
                ts9ToneState = toneA * ts9ToneState + (1.0f - toneA) * clipped;
                const float pedalLevel = juce::Decibels::decibelsToGain(-6.0f + level * 1.2f);
                processedInput = ts9ToneState * pedalLevel * 1.8f;
            }

            // Single-control guitar compressor. Sustain raises compression progressively;
            // timing and ratio stay tuned internally to keep pick attack and note decay natural.
            if (compressorActive)
            {
                processedInput = guitarCompressor.processSample(0, processedInput);
                const float sustain = compressorSustain.load(std::memory_order_relaxed);
                processedInput *= juce::Decibels::decibelsToGain(sustain * 0.22f);
            }

            // Bass Shift is an early-preamp control on the original IIC+.
            // Smooth its switch state here, before either NAM profile processes
            // the input, so both Rhythm and Lead profiles receive the same change.
            const float bassShiftTarget = bassShift.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
            const float bassShiftSmoothing = 1.0f - std::exp(-1.0f / (0.010f * (float)currentSampleRate));
            bassShiftMixCurrent += bassShiftSmoothing * (bassShiftTarget - bassShiftMixCurrent);
            const float bassShiftWet = bassShiftFilter.processSample(processedInput);
            processedInput += bassShiftMixCurrent * (bassShiftWet - processedInput);
            inputScratch[static_cast<size_t>(i)] = processedInput;
        }

        {
            std::lock_guard<std::mutex> lock(modelMutex);
            namInPtrs[0] = inputScratch.data();
            namOutPtrs[0] = outputScratch.data();
            cleanNamOutPtrs[0] = cleanOutputScratch.data();
            if (namModel)
                namModel->process(namInPtrs.data(), namOutPtrs.data(), n);
            else
                std::copy_n(inputScratch.data(), n, outputScratch.data());

            if (cleanNamModel)
                cleanNamModel->process(namInPtrs.data(), cleanNamOutPtrs.data(), n);
            else
                std::copy_n(outputScratch.data(), n, cleanOutputScratch.data());
        }

        // Pull Lead is the explicit channel selector. Both NAM models remain loaded
        // and processing, so the mode change does not reload models on the audio thread.
        // If the clean model is unavailable, the lead model remains active.
        const bool leadSelected = leadMode.load(std::memory_order_relaxed) || !cleanNamLoaded.load();
        const float cleanTarget = leadSelected ? 0.0f : 1.0f;
        // Linear crossfade reaches the target in approximately 200 ms, independent
        // of block size, for a more deliberate channel transition than the previous 10 ms.
        const float channelRampPerSample = 1.0f / (0.200f * (float)currentSampleRate);
        for (int i = 0; i < n; ++i)
        {
            if (cleanMixCurrent < cleanTarget)
                cleanMixCurrent = juce::jmin(cleanTarget, cleanMixCurrent + channelRampPerSample);
            else if (cleanMixCurrent > cleanTarget)
                cleanMixCurrent = juce::jmax(cleanTarget, cleanMixCurrent - channelRampPerSample);

            const float lead = outputScratch[static_cast<size_t>(i)];
            const float clean = cleanOutputScratch[static_cast<size_t>(i)];
            outputScratch[static_cast<size_t>(i)] = lead + cleanMixCurrent * (clean - lead);
        }
        cleanChannelMixForUi.store(cleanMixCurrent, std::memory_order_relaxed);

        // VOLUME remains post-NAM: it raises the captured amp level and gently
        // increases soft saturation as the control is turned up. MASTER remains
        // the independent final output trim. The tanh curve is normalized around
        // zero so the saturation adds progressively more peak rounding without
        // replacing the VOLUME control's normal level increase.
        const float volumeDb = leadMasterDb.load(std::memory_order_relaxed);
        const float volumeGain = juce::Decibels::decibelsToGain(volumeDb);
        const float volumePosition = juce::jlimit(0.0f, 1.0f, (volumeDb + 24.0f) / 36.0f);
        const float saturationDrive = 1.0f + 0.65f * volumePosition;
        const float saturationNorm = 1.0f / saturationDrive;
        for (int i = 0; i < n; ++i)
        {
            const float driven = outputScratch[static_cast<size_t>(i)] * volumeGain;
            outputScratch[static_cast<size_t>(i)] = std::tanh(driven * saturationDrive) * saturationNorm;
        }

        // Approximation layer: tone switches and five-band EQ after the captured amp.
        updateFilters();
        const float brightTarget = bright.load() ? 1.0f : 0.0f;
        const float deepTarget = deep.load() ? 1.0f : 0.0f;
        const float trebleShiftTarget = (trebleShift.load() && leadMode.load()) ? 1.0f : 0.0f;
        const float switchSmoothing = 1.0f - std::exp(-1.0f / (0.010f * (float)currentSampleRate));
        for (int i = 0; i < n; ++i)
        {
            brightMixCurrent += switchSmoothing * (brightTarget - brightMixCurrent);
            deepMixCurrent += switchSmoothing * (deepTarget - deepMixCurrent);
            trebleShiftMixCurrent += switchSmoothing * (trebleShiftTarget - trebleShiftMixCurrent);
            float x = outputScratch[static_cast<size_t>(i)];
            const float brightWet = brightFilter.processSample(x);
            x += brightMixCurrent * (brightWet - x);
            const float deepWet = deepFilter.processSample(x);
            x += deepMixCurrent * (deepWet - x);
            const float trebleShiftWet = trebleShiftFilter.processSample(x);
            const float trebleShiftControlled = trebleShiftHighCutFilter.processSample(trebleShiftWet);
            x += trebleShiftMixCurrent * (trebleShiftControlled - x);
            x = bassFilter.processSample(x);
            x = midFilter.processSample(x);
            x = trebleFilter.processSample(x);
            x = presenceFilter.processSample(x);
            x = eq80.processSample(x);
            x = eq240.processSample(x);
            x = eq750.processSample(x);
            x = eq2200.processSample(x);
            x = eq6600.processSample(x);
            outputScratch[static_cast<size_t>(i)] = x;
        }

        if (irModel)
        {
            irInPtrs[0] = outputScratch.data();
            irOutPtrs[0] = irScratch.data();
            irModel->process(irInPtrs.data(), irOutPtrs.data(), n);
        }
        else
        {
            std::copy_n(outputScratch.data(), n, irScratch.data());
        }

        // POST delay after cabinet/IR and before MASTER. A mono delay line feeds
        // both output channels; fractional linear interpolation and a smoothed
        // time target avoid abrupt jumps/clicks while changing TIME.
        const bool delayActive = delayEnabled.load(std::memory_order_relaxed);
        const float targetDelaySamples = juce::jlimit(1.0f, (float)delayBuffer.size() - 2.0f,
            delayTimeMs.load(std::memory_order_relaxed) * 0.001f * (float)currentSampleRate);
        const float feedback = delayFeedback.load(std::memory_order_relaxed);
        const float mix = delayMix.load(std::memory_order_relaxed);
        const float timeSmooth = 1.0f - std::exp(-1.0f / (0.025f * (float)currentSampleRate));
        const float lpA = std::exp(-2.0f * juce::MathConstants<float>::pi * 5200.0f / (float)currentSampleRate);
        for (int i = 0; i < n; ++i)
        {
            delayTimeSamplesCurrent += timeSmooth * (targetDelaySamples - delayTimeSamplesCurrent);
            const float dry = irScratch[static_cast<size_t>(i)];
            float readPosition = (float)delayWriteIndex - delayTimeSamplesCurrent;
            while (readPosition < 0.0f) readPosition += (float)delayBuffer.size();
            const auto indexA = (size_t)readPosition % delayBuffer.size();
            const auto indexB = (indexA + 1) % delayBuffer.size();
            const float fraction = readPosition - std::floor(readPosition);
            const float delayed = delayBuffer[indexA] + fraction * (delayBuffer[indexB] - delayBuffer[indexA]);
            delayLowpassState = lpA * delayLowpassState + (1.0f - lpA) * delayed;
            delayBuffer[delayWriteIndex] = juce::jlimit(-2.0f, 2.0f, dry + delayLowpassState * feedback);
            delayWriteIndex = (delayWriteIndex + 1) % delayBuffer.size();
            irScratch[static_cast<size_t>(i)] = delayActive ? dry * (1.0f - mix) + delayed * mix : dry;
        }

        // Stereo post-delay chorus. Two modulated short delay taps use a
        // quarter-cycle phase offset to create width without pitchy hard panning.
        // Buffers are allocated in prepare(), so the audio callback never allocates.
        const float chorusEnabledTarget = chorusEnabled.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        const float chorusRateTarget = chorusRate.load(std::memory_order_relaxed);
        const float chorusDepthTarget = chorusDepth.load(std::memory_order_relaxed);
        const float chorusMixTarget = chorusMix.load(std::memory_order_relaxed);
        const float chorusSmooth = 1.0f - std::exp(-1.0f / (0.020f * (float)currentSampleRate));
        const float outGain = juce::Decibels::decibelsToGain(outputGainDb.load());
        const size_t chorusSize = chorusBuffers[0].size();
        const float baseDelaySamples = 0.020f * (float)currentSampleRate;
        for (int i = 0; i < n; ++i)
        {
            chorusEnableMixCurrent += chorusSmooth * (chorusEnabledTarget - chorusEnableMixCurrent);
            chorusRateCurrent += chorusSmooth * (chorusRateTarget - chorusRateCurrent);
            chorusDepthCurrent += chorusSmooth * (chorusDepthTarget - chorusDepthCurrent);
            chorusMixCurrent += chorusSmooth * (chorusMixTarget - chorusMixCurrent);
            const float dry = irScratch[static_cast<size_t>(i)];
            const float depthSamples = chorusDepthCurrent * 0.001f * (float)currentSampleRate;
            const float phaseAdvance = juce::MathConstants<double>::twoPi * (double)chorusRateCurrent / currentSampleRate;
            float channelSamples[2] { dry, dry };
            for (int ch = 0; ch < 2; ++ch)
            {
                const double phase = chorusPhase + (ch == 0 ? 0.0 : juce::MathConstants<double>::halfPi);
                const float lfo = (float)std::sin(phase);
                const float delaySamples = juce::jlimit(2.0f, (float)chorusSize - 2.0f,
                    baseDelaySamples + depthSamples * lfo);
                float readPosition = (float)chorusWriteIndex - delaySamples;
                while (readPosition < 0.0f) readPosition += (float)chorusSize;
                const size_t indexA = static_cast<size_t>(readPosition) % chorusSize;
                const size_t indexB = (indexA + 1) % chorusSize;
                const float fraction = readPosition - std::floor(readPosition);
                const float wet = chorusBuffers[static_cast<size_t>(ch)][indexA]
                    + fraction * (chorusBuffers[static_cast<size_t>(ch)][indexB] - chorusBuffers[static_cast<size_t>(ch)][indexA]);
                chorusBuffers[static_cast<size_t>(ch)][chorusWriteIndex] = dry;
                channelSamples[ch] = dry + chorusEnableMixCurrent * chorusMixCurrent * (wet - dry);
            }
            chorusWriteIndex = (chorusWriteIndex + 1) % chorusSize;
            chorusPhase += phaseAdvance;
            if (chorusPhase >= juce::MathConstants<double>::twoPi)
                chorusPhase -= juce::MathConstants<double>::twoPi;

            reverbScratch[0][static_cast<size_t>(i)] = channelSamples[0];
            reverbScratch[1][static_cast<size_t>(i)] = channelSamples[1];
            reverbDryScratch[0][static_cast<size_t>(i)] = channelSamples[0];
            reverbDryScratch[1][static_cast<size_t>(i)] = channelSamples[1];
        }

        // Stereo post-chorus reverb. A short pre-delay is applied to the wet
        // feed only; the direct signal remains immediate. JUCE's reverb network
        // is prepared ahead of time and does not allocate in the audio callback.
        const bool reverbActive = reverbEnabled.load(std::memory_order_relaxed);
        const float targetReverbMix = reverbActive ? reverbMix.load(std::memory_order_relaxed) * 0.01f : 0.0f;
        const float reverbSmooth = 1.0f - std::exp(-(float)n / (0.025f * (float)currentSampleRate));
        reverbMixCurrent += reverbSmooth * (targetReverbMix - reverbMixCurrent);
        const float targetPreDelay = reverbPreDelay.load(std::memory_order_relaxed) * 0.001f * (float)currentSampleRate;
        const float preDelaySmooth = 1.0f - std::exp(-1.0f / (0.020f * (float)currentSampleRate));
        const float size = reverbSize.load(std::memory_order_relaxed) / 10.0f;
        const float decay = reverbDecay.load(std::memory_order_relaxed);
        const int type = reverbType.load(std::memory_order_relaxed);
        const float typeRoomOffset = type == 0 ? -0.10f : (type == 2 ? 0.06f : (type == 3 ? 0.13f : 0.0f));
        const float room = juce::jlimit(0.15f, 0.98f, 0.22f + 0.56f * (decay / 8.0f) + 0.18f * size + typeRoomOffset);
        // Fixed, neutral damping keeps the reverb naturally voiced; there is no user tone/EQ control.
        const float damping = 0.42f;
        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = room;
        reverbParams.damping = damping;
        reverbParams.wetLevel = 1.0f;
        reverbParams.dryLevel = 0.0f;
        reverbParams.width = type == 1 ? 0.82f : 1.0f;
        reverbParams.freezeMode = false;
        reverb.setParameters(reverbParams);

        const size_t preDelaySize = reverbPreDelayBuffers[0].size();
        for (int i = 0; i < n; ++i)
        {
            reverbPreDelaySamplesCurrent += preDelaySmooth * (targetPreDelay - reverbPreDelaySamplesCurrent);
            const float delaySamples = juce::jlimit(0.0f, (float)preDelaySize - 2.0f, reverbPreDelaySamplesCurrent);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& buffer = reverbPreDelayBuffers[static_cast<size_t>(ch)];
                const float input = reverbScratch[static_cast<size_t>(ch)][static_cast<size_t>(i)];
                buffer[reverbPreDelayWriteIndex] = input;
                float readPosition = (float)reverbPreDelayWriteIndex - delaySamples;
                while (readPosition < 0.0f) readPosition += (float)preDelaySize;
                const size_t indexA = static_cast<size_t>(readPosition) % preDelaySize;
                const size_t indexB = (indexA + 1) % preDelaySize;
                const float fraction = readPosition - std::floor(readPosition);
                reverbScratch[static_cast<size_t>(ch)][static_cast<size_t>(i)] =
                    buffer[indexA] + fraction * (buffer[indexB] - buffer[indexA]);
            }
            reverbPreDelayWriteIndex = (reverbPreDelayWriteIndex + 1) % preDelaySize;
        }

        reverb.processStereo(reverbScratch[0].data(), reverbScratch[1].data(), n);
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] == nullptr) continue;
                const int channel = juce::jmin(ch, 1);
                const float wet = reverbScratch[static_cast<size_t>(channel)][static_cast<size_t>(i)];
                // The reverb receives the pre-delayed signal; use a separate dry
                // tap so PRE-DELAY affects only the reverberant field.
                const float dry = reverbDryScratch[static_cast<size_t>(channel)][static_cast<size_t>(i)];
                const float effected = dry * (1.0f - reverbMixCurrent) + wet * reverbMixCurrent;
                const float outputSample = effected * outGain;
                const float outputAbs = std::abs(outputSample);
                blockOutputPeak = juce::jmax(blockOutputPeak, outputAbs);
                blockOutputClip = blockOutputClip || outputAbs >= 0.999f;
                outputChannelData[ch][offset + i] = outputSample;
            }
        }
        offset += n;
    }
    inputPeakLinear.store(blockInputPeak);
    outputPeakLinear.store(blockOutputPeak);
    if (blockInputClip) inputClip.store(true);
    if (blockOutputClip) outputClip.store(true);
}
