#include "ProceduralAudio.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace ProceduralAudio {

static constexpr float PI = 3.14159265358979f;
static constexpr float TWO_PI = 6.28318530717959f;

// ============================================================
// Helper functions
// ============================================================

std::vector<float> generateSineWave(float frequency, float duration, float sampleRate) {
    size_t numSamples = (size_t)(duration * sampleRate);
    std::vector<float> samples(numSamples, 0.0f);

    float angularFreq = TWO_PI * frequency;
    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;
        samples[i] = sinf(angularFreq * t);
    }

    return samples;
}

std::vector<float> generateWhiteNoise(float duration, float sampleRate) {
    size_t numSamples = (size_t)(duration * sampleRate);
    std::vector<float> samples(numSamples, 0.0f);

    for (size_t i = 0; i < numSamples; i++) {
        samples[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    }

    return samples;
}

std::vector<float> mixWaves(const std::vector<float>& a, const std::vector<float>& b, float mix) {
    size_t maxLen = std::max(a.size(), b.size());
    std::vector<float> result(maxLen, 0.0f);

    for (size_t i = 0; i < maxLen; i++) {
        float sampleA = (i < a.size()) ? a[i] : 0.0f;
        float sampleB = (i < b.size()) ? b[i] : 0.0f;
        result[i] = sampleA * (1.0f - mix) + sampleB * mix;
    }

    return result;
}

// Helper: create a Wave from float samples (32-bit float, mono, 44100Hz)
static Wave createWaveFromFloats(const std::vector<float>& samples, float sampleRate = 44100.0f) {
    Wave wave = { 0 };
    wave.frameCount = (unsigned int)samples.size();
    wave.sampleRate = (unsigned int)sampleRate;
    wave.sampleSize = 32; // 32-bit float
    wave.channels = 1;

    size_t dataSize = samples.size() * sizeof(float);
    wave.data = RL_MALLOC(dataSize);
    if (wave.data) {
        memcpy(wave.data, samples.data(), dataSize);
    }

    return wave;
}

// Helper: apply an envelope to a sample buffer
// attack: time in seconds to reach full amplitude
// decay: time in seconds to decay from full amplitude to sustain
// sustain: sustain level (0-1)
// release: time in seconds from sustain to zero
static void applyADSR(std::vector<float>& samples, float sampleRate,
                       float attack, float decay, float sustain, float release) {
    size_t attackSamples = (size_t)(attack * sampleRate);
    size_t decaySamples = (size_t)(decay * sampleRate);
    size_t releaseSamples = (size_t)(release * sampleRate);
    size_t totalSamples = samples.size();

    for (size_t i = 0; i < totalSamples; i++) {
        float envelope = 1.0f;

        // Attack phase
        if (i < attackSamples) {
            envelope = (float)i / (float)attackSamples;
        }
        // Decay phase
        else if (i < attackSamples + decaySamples) {
            float decayProgress = (float)(i - attackSamples) / (float)decaySamples;
            envelope = 1.0f - (1.0f - sustain) * decayProgress;
        }
        // Sustain phase
        else if (i < totalSamples - releaseSamples) {
            envelope = sustain;
        }
        // Release phase
        else if (releaseSamples > 0) {
            float releaseProgress = (float)(i - (totalSamples - releaseSamples)) / (float)releaseSamples;
            envelope = sustain * (1.0f - releaseProgress);
        }

        samples[i] *= envelope;
    }
}

// Helper: apply a low-pass filter (simple moving average)
static std::vector<float> lowPassFilter(const std::vector<float>& input, int windowSize) {
    std::vector<float> output(input.size(), 0.0f);
    float sum = 0.0f;

    for (size_t i = 0; i < input.size(); i++) {
        sum += input[i];
        if (i >= (size_t)windowSize) {
            sum -= input[i - windowSize];
        }
        int count = (int)((i < (size_t)windowSize) ? i + 1 : windowSize);
        output[i] = sum / count;
    }

    return output;
}

// ============================================================
// Sound generators
// ============================================================

Wave generateHeartbeat(float bpm) {
    // Two low-frequency (60Hz) thump sounds in quick succession,
    // then silence for the rest of the beat period based on BPM.
    // The thump is a sine wave with fast attack and medium decay envelope.

    float sampleRate = 44100.0f;
    float beatDuration = 60.0f / bpm; // Duration of one beat in seconds
    float totalDuration = beatDuration; // One full beat cycle

    size_t numSamples = (size_t)(totalDuration * sampleRate);
    std::vector<float> samples(numSamples, 0.0f);

    // First thump: starts at 0.0s, lasts ~0.1s
    float thump1Start = 0.0f;
    float thump1Duration = 0.1f;
    size_t thump1StartSample = (size_t)(thump1Start * sampleRate);
    size_t thump1EndSample = std::min((size_t)((thump1Start + thump1Duration) * sampleRate), numSamples);

    for (size_t i = thump1StartSample; i < thump1EndSample; i++) {
        float t = (float)(i - thump1StartSample) / sampleRate;
        // Low frequency sine with harmonic
        float thump = sinf(TWO_PI * 60.0f * t) * 0.8f;
        thump += sinf(TWO_PI * 30.0f * t) * 0.3f; // Sub-bass

        // Fast attack, medium decay envelope
        float progress = t / thump1Duration;
        float envelope = expf(-progress * 8.0f);
        envelope *= std::min(progress * 40.0f, 1.0f); // Fast attack

        samples[i] += thump * envelope * 0.7f;
    }

    // Second thump: starts at 0.15s, lasts ~0.08s (slightly shorter, quieter)
    float thump2Start = 0.15f;
    float thump2Duration = 0.08f;
    size_t thump2StartSample = (size_t)(thump2Start * sampleRate);
    size_t thump2EndSample = std::min((size_t)((thump2Start + thump2Duration) * sampleRate), numSamples);

    for (size_t i = thump2StartSample; i < thump2EndSample; i++) {
        float t = (float)(i - thump2StartSample) / sampleRate;
        // Higher frequency for the second beat (the "dub" of "lub-dub")
        float thump = sinf(TWO_PI * 80.0f * t) * 0.6f;
        thump += sinf(TWO_PI * 40.0f * t) * 0.2f;

        float progress = t / thump2Duration;
        float envelope = expf(-progress * 10.0f);
        envelope *= std::min(progress * 50.0f, 1.0f);

        samples[i] += thump * envelope * 0.5f;
    }

    // Rest of the beat is silence (already initialized to 0.0f)

    return createWaveFromFloats(samples, sampleRate);
}

Wave generateFootstep() {
    // Short burst of filtered white noise (100ms) with fast attack and decay.
    // Mix with a low thump for impact.

    float sampleRate = 44100.0f;
    float duration = 0.1f; // 100ms
    size_t numSamples = (size_t)(duration * sampleRate);

    // Generate white noise
    auto noise = generateWhiteNoise(duration, sampleRate);

    // Apply low-pass filter to make it sound like a footstep
    noise = lowPassFilter(noise, 100);

    // Generate low thump
    auto thump = generateSineWave(60.0f, duration, sampleRate);

    // Apply envelope to thump: fast attack, quick decay
    applyADSR(thump, sampleRate, 0.002f, 0.02f, 0.2f, 0.03f);

    // Mix noise and thump
    auto mixed = mixWaves(noise, thump, 0.4f);

    // Apply overall envelope
    applyADSR(mixed, sampleRate, 0.001f, 0.03f, 0.3f, 0.05f);

    // Normalize
    float maxVal = 0.0f;
    for (auto& s : mixed) {
        maxVal = std::max(maxVal, std::abs(s));
    }
    if (maxVal > 0.0f) {
        for (auto& s : mixed) {
            s /= maxVal;
            s *= 0.8f; // Scale to reasonable volume
        }
    }

    return createWaveFromFloats(mixed, sampleRate);
}

Wave generateDoorCreak() {
    // Frequency sweep from 200Hz to 800Hz over 1 second with amplitude modulation.

    float sampleRate = 44100.0f;
    float duration = 1.0f;
    size_t numSamples = (size_t)(duration * sampleRate);
    std::vector<float> samples(numSamples, 0.0f);

    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;
        float progress = t / duration;

        // Frequency sweep: 200Hz -> 800Hz
        float freq = 200.0f + progress * 600.0f;

        // Sine wave with frequency sweep
        // Use phase accumulation for smooth sweep
        float sample = sinf(TWO_PI * freq * t);

        // Amplitude modulation: create rhythmic creaking
        float amFreq = 8.0f + progress * 12.0f; // Speed up the creak
        float am = 0.5f + 0.5f * sinf(TWO_PI * amFreq * t);

        // Overall envelope: fade in, sustain, fade out
        float envelope = 1.0f;
        if (progress < 0.05f) {
            envelope = progress / 0.05f;
        } else if (progress > 0.85f) {
            envelope = (1.0f - progress) / 0.15f;
        }

        samples[i] = sample * am * envelope * 0.5f;
    }

    // Add some noise for texture
    auto noise = generateWhiteNoise(duration, sampleRate);
    noise = lowPassFilter(noise, 50);
    for (size_t i = 0; i < numSamples; i++) {
        samples[i] += noise[i] * 0.1f;
    }

    return createWaveFromFloats(samples, sampleRate);
}

Wave generateWhisper(float duration) {
    // Filtered white noise with amplitude modulation to create whisper-like sound.

    float sampleRate = 44100.0f;
    size_t numSamples = (size_t)(duration * sampleRate);

    // Generate base white noise
    auto noise = generateWhiteNoise(duration, sampleRate);

    // Apply band-pass-like filtering: low-pass then emphasize mid-range
    noise = lowPassFilter(noise, 200);

    // Add amplitude modulation for whisper-like breathing pattern
    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;

        // Breathing-like amplitude modulation
        float breathRate = 3.0f + sinf(t * 0.5f) * 0.5f; // Varying breath rate
        float am = 0.3f + 0.7f * (0.5f + 0.5f * sinf(TWO_PI * breathRate * t));

        // Overall envelope: fade in and out
        float progress = t / duration;
        float envelope = 1.0f;
        if (progress < 0.1f) {
            envelope = progress / 0.1f;
        } else if (progress > 0.8f) {
            envelope = (1.0f - progress) / 0.2f;
        }

        noise[i] *= am * envelope * 0.3f;
    }

    // Add subtle formant-like resonance (simple sine waves at speech formant frequencies)
    auto formant1 = generateSineWave(500.0f, duration, sampleRate); // First formant
    auto formant2 = generateSineWave(1500.0f, duration, sampleRate); // Second formant

    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;
        float progress = t / duration;

        // Modulate formants with different patterns
        float f1Gain = 0.03f * (0.5f + 0.5f * sinf(TWO_PI * 4.0f * t));
        float f2Gain = 0.015f * (0.5f + 0.5f * sinf(TWO_PI * 6.0f * t + 1.0f));

        float envelope = 1.0f;
        if (progress < 0.1f) envelope = progress / 0.1f;
        else if (progress > 0.8f) envelope = (1.0f - progress) / 0.2f;

        noise[i] += (formant1[i] * f1Gain + formant2[i] * f2Gain) * envelope;
    }

    return createWaveFromFloats(noise, sampleRate);
}

Wave generateStatic(float duration) {
    // Pure white noise at low volume.

    float sampleRate = 44100.0f;
    size_t numSamples = (size_t)(duration * sampleRate);

    auto noise = generateWhiteNoise(duration, sampleRate);

    // Apply subtle volume variation for more natural static
    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;

        // Random volume fluctuation
        float volMod = 0.8f + 0.2f * sinf(TWO_PI * 0.5f * t + (float)i * 0.01f);

        // Envelope
        float progress = t / duration;
        float envelope = 1.0f;
        if (progress < 0.05f) envelope = progress / 0.05f;
        else if (progress > 0.9f) envelope = (1.0f - progress) / 0.1f;

        noise[i] *= 0.2f * volMod * envelope;
    }

    return createWaveFromFloats(noise, sampleRate);
}

Wave generateScream() {
    // Mix of high-frequency sine waves (800-2000Hz) with fast attack, designed to be jarring.

    float sampleRate = 44100.0f;
    float duration = 0.8f;
    size_t numSamples = (size_t)(duration * sampleRate);
    std::vector<float> samples(numSamples, 0.0f);

    // Generate multiple high-frequency sine waves
    float frequencies[] = { 800.0f, 1200.0f, 1600.0f, 2000.0f, 2400.0f };
    float amplitudes[] = { 0.4f, 0.3f, 0.25f, 0.2f, 0.15f };

    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;
        float sample = 0.0f;

        for (int f = 0; f < 5; f++) {
            // Each frequency with slight detuning for harshness
            float detune = 1.0f + sinf(t * 10.0f + f) * 0.02f;
            sample += sinf(TWO_PI * frequencies[f] * detune * t) * amplitudes[f];
        }

        // Fast attack, sustained then quick release
        float progress = t / duration;
        float envelope;
        if (progress < 0.01f) {
            envelope = progress / 0.01f; // Very fast attack (10ms)
        } else if (progress < 0.6f) {
            envelope = 1.0f; // Sustain
        } else {
            envelope = (1.0f - progress) / 0.4f; // Release
        }

        // Add harshness: rapid amplitude modulation
        float harshAM = 0.7f + 0.3f * sinf(TWO_PI * 30.0f * t);
        if (progress < 0.1f) harshAM = 1.0f; // No AM at attack for cleaner hit

        samples[i] = sample * envelope * harshAM * 0.6f;
    }

    // Add noise burst for texture
    auto noise = generateWhiteNoise(duration, sampleRate);
    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;
        float progress = t / duration;

        // Noise is loudest at the attack
        float noiseEnvelope = expf(-progress * 5.0f);
        samples[i] += noise[i] * noiseEnvelope * 0.3f;
    }

    // Normalize
    float maxVal = 0.0f;
    for (auto& s : samples) {
        maxVal = std::max(maxVal, std::abs(s));
    }
    if (maxVal > 0.0f) {
        for (auto& s : samples) {
            s = s / maxVal * 0.9f;
        }
    }

    return createWaveFromFloats(samples, sampleRate);
}

Wave generateWind() {
    // Low-frequency noise with slow amplitude modulation.

    float sampleRate = 44100.0f;
    float duration = 3.0f; // 3 second wind sound
    size_t numSamples = (size_t)(duration * sampleRate);

    // Generate white noise
    auto noise = generateWhiteNoise(duration, sampleRate);

    // Heavy low-pass filter for wind-like sound
    noise = lowPassFilter(noise, 300);

    // Second pass for even smoother sound
    noise = lowPassFilter(noise, 150);

    // Apply slow amplitude modulation for howling wind effect
    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;

        // Slow modulation (howling)
        float slowMod = 0.5f + 0.5f * sinf(TWO_PI * 0.3f * t);
        // Medium modulation (gusting)
        float medMod = 0.7f + 0.3f * sinf(TWO_PI * 1.2f * t + 0.5f);

        float envelope = 1.0f;
        float progress = t / duration;
        if (progress < 0.05f) envelope = progress / 0.05f;
        else if (progress > 0.9f) envelope = (1.0f - progress) / 0.1f;

        noise[i] *= slowMod * medMod * envelope * 0.4f;
    }

    // Add subtle low-frequency rumble
    auto rumble = generateSineWave(40.0f, duration, sampleRate);
    for (size_t i = 0; i < numSamples; i++) {
        float t = (float)i / sampleRate;
        float progress = t / duration;
        float envelope = 1.0f;
        if (progress < 0.05f) envelope = progress / 0.05f;
        else if (progress > 0.9f) envelope = (1.0f - progress) / 0.1f;

        noise[i] += rumble[i] * 0.08f * envelope;
    }

    return createWaveFromFloats(noise, sampleRate);
}

Wave generateDripping() {
    // Short high-frequency blip (3000Hz sine, 50ms) repeated at random intervals.

    float sampleRate = 44100.0f;
    float totalDuration = 2.0f; // 2 seconds of dripping
    size_t totalSamples = (size_t)(totalDuration * sampleRate);
    std::vector<float> samples(totalSamples, 0.0f);

    // Generate drip at specific times
    float dripTimes[] = { 0.0f, 0.3f, 0.7f, 1.1f, 1.6f };
    int numDrips = 5;
    float dripDuration = 0.05f; // 50ms per drip
    size_t dripSamples = (size_t)(dripDuration * sampleRate);

    for (int d = 0; d < numDrips; d++) {
        size_t startSample = (size_t)(dripTimes[d] * sampleRate);

        for (size_t j = 0; j < dripSamples && (startSample + j) < totalSamples; j++) {
            float t = (float)j / sampleRate;
            float progress = t / dripDuration;

            // High-frequency sine with fast decay
            float drip = sinf(TWO_PI * 3000.0f * t);
            // Add a bit of lower frequency for body
            drip += sinf(TWO_PI * 1500.0f * t) * 0.3f;

            // Envelope: very fast attack, exponential decay
            float envelope = expf(-progress * 20.0f);
            envelope *= std::min(progress * 100.0f, 1.0f); // Fast attack

            // Slightly vary pitch and volume per drip
            float pitchVar = 1.0f + (d - 2) * 0.05f;
            float volVar = 0.7f + (float)(d % 3) * 0.1f;

            samples[startSample + j] += drip * envelope * 0.4f * volVar;
        }

        // Add a tiny splash noise after each drip
        size_t splashStart = startSample + (size_t)(0.01f * sampleRate);
        size_t splashLen = (size_t)(0.02f * sampleRate);
        for (size_t j = 0; j < splashLen && (splashStart + j) < totalSamples; j++) {
            float t = (float)j / sampleRate;
            float progress = t / 0.02f;
            float splashNoise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            float envelope = expf(-progress * 30.0f);
            samples[splashStart + j] += splashNoise * envelope * 0.1f;
        }
    }

    return createWaveFromFloats(samples, sampleRate);
}

} // namespace ProceduralAudio
