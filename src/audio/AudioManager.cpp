#include "AudioManager.h"
#include "ProceduralAudio.h"
#include "../utils/Constants.h"
#include <cmath>
#include <algorithm>

using namespace Kaikai;

AudioManager::AudioManager()
    : heartbeatSound{0}
    , footstepSound{0}
    , doorSound{0}
    , ambientSound{0}
    , backgroundMusic{0}
{
}

AudioManager::~AudioManager() {
    if (initialized) {
        shutdown();
    }
}

void AudioManager::init() {
    if (initialized) return;

    // Try to initialize audio device - this may fail on some Android devices
    InitAudioDevice();

    // Safety: check if audio device is ready
    if (!IsAudioDeviceReady()) {
        // Audio not available - game will run without sound
        return;
    }

    try {
        // Generate procedural sounds
        Wave heartbeatWave = ProceduralAudio::generateHeartbeat(60.0f);
        heartbeatSound = LoadSoundFromWave(heartbeatWave);
        UnloadWave(heartbeatWave);

        Wave footstepWave = ProceduralAudio::generateFootstep();
        footstepSound = LoadSoundFromWave(footstepWave);
        UnloadWave(footstepWave);

        Wave doorWave = ProceduralAudio::generateDoorCreak();
        doorSound = LoadSoundFromWave(doorWave);
        UnloadWave(doorWave);

        Wave ambientWave = ProceduralAudio::generateWind();
        ambientSound = LoadSoundFromWave(ambientWave);
        UnloadWave(ambientWave);

        SetSoundVolume(heartbeatSound, 0.0f); // Start silent, controlled by ghost proximity
        SetSoundVolume(footstepSound, 0.6f);
        SetSoundVolume(doorSound, 0.5f);
        SetSoundVolume(ambientSound, 0.3f);
    } catch (...) {
        // Sound generation failed - not fatal, game runs without audio
        return;
    }

    initialized = true;
}

void AudioManager::shutdown() {
    if (!initialized) return;

    UnloadSound(heartbeatSound);
    UnloadSound(footstepSound);
    UnloadSound(doorSound);
    UnloadSound(ambientSound);

    if (backgroundMusic.stream.sampleRate != 0) {
        StopMusicStream(backgroundMusic);
        UnloadMusicStream(backgroundMusic);
    }

    CloseAudioDevice();
    initialized = false;
}

void AudioManager::update(float deltaTime) {
    // Update heartbeat timer
    if (heartbeatBPM > 0.0f) {
        float beatInterval = 60.0f / heartbeatBPM;
        heartbeatTimer += deltaTime;

        if (heartbeatTimer >= beatInterval) {
            heartbeatTimer -= beatInterval;
            if (heartbeatTimer > beatInterval) {
                heartbeatTimer = 0.0f; // Reset if we fell too far behind
            }

            // Play heartbeat sound
            if (initialized) {
                SetSoundPitch(heartbeatSound, 0.8f + (heartbeatBPM - 60.0f) / 120.0f * 0.4f);
                PlaySound(heartbeatSound);
            }
        }
    }

    // Update ambient timer
    ambientTimer += deltaTime;

    // Update background music stream
    if (backgroundMusic.stream.sampleRate != 0) {
        UpdateMusicStream(backgroundMusic);
    }
}

void AudioManager::playSound(const std::string& name, float volume, float pitch) {
    if (!initialized) return;

    // Map sound names to generated sounds
    Sound* targetSound = nullptr;

    if (name == "heartbeat") {
        targetSound = &heartbeatSound;
    } else if (name == "footstep") {
        targetSound = &footstepSound;
    } else if (name == "door") {
        targetSound = &doorSound;
    } else if (name == "ambient") {
        targetSound = &ambientSound;
    }

    if (targetSound) {
        SetSoundVolume(*targetSound, std::clamp(volume, 0.0f, 1.0f));
        SetSoundPitch(*targetSound, std::clamp(pitch, 0.1f, 3.0f));
        PlaySound(*targetSound);
    }
}

void AudioManager::playSound3D(const std::string& name, Vector3 position, Vector3 listenerPos, float maxDistance) {
    if (!initialized) return;

    float volume = calculateVolumeFromDistance(position, listenerPos, maxDistance);
    if (volume <= 0.01f) return; // Too far away, don't play

    playSound(name, volume, 1.0f);
}

void AudioManager::playMusic(const std::string& name, float volume) {
    if (!initialized) return;

    // Stop existing music if playing
    if (backgroundMusic.stream.sampleRate != 0) {
        StopMusicStream(backgroundMusic);
        UnloadMusicStream(backgroundMusic);
    }

    // In a full implementation, we would load music from file:
    // backgroundMusic = LoadMusicStream(name.c_str());
    //
    // For now, generate a procedural ambient music using wind + whispers
    Wave musicWave = ProceduralAudio::generateWind();
    // Convert Wave to Music stream is not directly supported in raylib,
    // so we use the ambient sound as a looping background
    UnloadWave(musicWave);

    // Use ambient sound loop as background
    SetSoundVolume(ambientSound, volume);
    PlaySound(ambientSound);
}

void AudioManager::stopMusic() {
    if (!initialized) return;

    if (backgroundMusic.stream.sampleRate != 0) {
        StopMusicStream(backgroundMusic);
    }
    StopSound(ambientSound);
}

void AudioManager::updateProximityAudio(const std::vector<std::pair<uint32_t, Vector3>>& otherPlayers, Vector3 localPos) {
    if (!initialized) return;

    // Update volume levels for each player based on distance
    for (const auto& [playerId, playerPos] : otherPlayers) {
        float volume = calculateVolumeFromDistance(playerPos, localPos, 30.0f);
        playerVolumes[playerId] = volume;

        // In a full implementation, this would adjust voice chat volume:
        // voiceChat.setPlayerVolume(playerId, volume);
    }
}

void AudioManager::playAmbient() {
    if (!initialized) return;

    // Play random horror ambient sounds at intervals
    // Creaking, dripping, wind
    float ambientInterval = 8.0f + (float)(rand() % 100) / 100.0f * 12.0f; // 8-20 seconds

    if (ambientTimer >= ambientInterval) {
        ambientTimer = 0.0f;

        // Randomly choose an ambient sound type
        int choice = rand() % 3;

        switch (choice) {
            case 0: {
                // Wind sound
                Wave wave = ProceduralAudio::generateWind();
                Sound snd = LoadSoundFromWave(wave);
                UnloadWave(wave);
                SetSoundVolume(snd, 0.15f);
                PlaySound(snd);
                // Note: In production, we'd track this sound to unload it later
                break;
            }
            case 1: {
                // Dripping sound
                Wave wave = ProceduralAudio::generateDripping();
                Sound snd = LoadSoundFromWave(wave);
                UnloadWave(wave);
                SetSoundVolume(snd, 0.25f);
                PlaySound(snd);
                break;
            }
            case 2: {
                // Door creak
                SetSoundVolume(doorSound, 0.15f);
                PlaySound(doorSound);
                break;
            }
        }
    }
}

void AudioManager::updateAmbient(float sanity) {
    if (!initialized) return;

    // At low sanity, ambient sounds become more frequent and disturbing
    playAmbient();

    // Adjust ambient sound properties based on sanity
    float sanityFraction = sanity / DEFAULT_SANITY;

    if (sanityFraction < 0.4f) {
        // Low sanity: more frequent, more disturbing ambience
        // Play whisper sounds with probability based on sanity deficit
        float whisperProbability = (1.0f - sanityFraction) * 0.02f; // ~2% per frame at lowest sanity
        float roll = (float)(rand() % 10000) / 10000.0f;
        if (roll < whisperProbability) {
            Wave whisperWave = ProceduralAudio::generateWhisper(2.0f);
            Sound whisperSnd = LoadSoundFromWave(whisperWave);
            UnloadWave(whisperWave);
            SetSoundVolume(whisperSnd, (1.0f - sanityFraction) * 0.3f);
            PlaySound(whisperSnd);
        }

        // Play static sounds at very low sanity
        if (sanityFraction < 0.2f) {
            float staticProbability = (1.0f - sanityFraction) * 0.01f;
            float staticRoll = (float)(rand() % 10000) / 10000.0f;
            if (staticRoll < staticProbability) {
                Wave staticWave = ProceduralAudio::generateStatic(1.0f);
                Sound staticSnd = LoadSoundFromWave(staticWave);
                UnloadWave(staticWave);
                SetSoundVolume(staticSnd, (1.0f - sanityFraction) * 0.15f);
                PlaySound(staticSnd);
            }
        }
    }

    // Adjust background ambient volume
    float ambientVolume = 0.15f + (1.0f - sanityFraction) * 0.2f;
    SetSoundVolume(ambientSound, ambientVolume);
}

void AudioManager::playFootstep(Vector3 position, bool running) {
    if (!initialized) return;

    float volume = running ? 0.7f : 0.4f;
    float pitch = running ? 1.1f : 0.9f;

    SetSoundVolume(footstepSound, volume);
    SetSoundPitch(footstepSound, pitch);
    PlaySound(footstepSound);
}

void AudioManager::updateFootstepSounds(float deltaTime, const PlayerState& player) {
    if (!initialized) return;
    if (player.isDead || player.isSpectator) return;

    // Determine if the player is moving
    // In a full implementation, we'd check velocity or position delta
    // For now, we assume movement is happening if footstepTimer is running
    // The caller should set a flag or pass velocity information

    // Footstep intervals: walk every 0.5s, run every 0.3s
    bool isRunning = !player.isDead && player.stamina > 10.0f; // Simplified check
    float stepInterval = isRunning ? 0.3f : 0.5f;

    footstepTimer += deltaTime;

    if (footstepTimer >= stepInterval) {
        footstepTimer -= stepInterval;
        if (footstepTimer > stepInterval) {
            footstepTimer = 0.0f;
        }
        playFootstep(player.position, isRunning);
    }
}

void AudioManager::updateGhostProximity(float ghostDistance) {
    if (!initialized) return;

    // As ghost gets closer:
    // - Heartbeat BPM increases (60 at far distance, 180 at very close)
    // - Heartbeat volume increases
    // - Add subtle whisper/static sounds that get louder

    float maxDist = 30.0f; // Maximum distance for ghost audio effects
    float minDist = 1.0f;  // Distance for maximum intensity

    if (ghostDistance > maxDist) {
        // Ghost is too far for audio effects
        setHeartbeatRate(60.0f);
        SetSoundVolume(heartbeatSound, 0.0f);
        return;
    }

    // Calculate proximity factor: 0.0 at maxDist, 1.0 at minDist
    float proximityFactor = 1.0f - std::clamp((ghostDistance - minDist) / (maxDist - minDist), 0.0f, 1.0f);
    proximityFactor = proximityFactor * proximityFactor; // Quadratic for more dramatic buildup

    // Heartbeat BPM: 60 at far, 180 at close
    float targetBPM = 60.0f + proximityFactor * 120.0f;
    setHeartbeatRate(targetBPM);

    // Heartbeat volume: quiet at far, loud at close
    float heartbeatVolume = proximityFactor * 0.8f;
    SetSoundVolume(heartbeatSound, heartbeatVolume);

    // Whisper sounds when ghost is moderately close
    if (ghostDistance < 15.0f && ghostDistance > 5.0f) {
        float whisperIntensity = (1.0f - ghostDistance / 15.0f) * 0.3f;
        // Periodically play whisper sounds based on proximity
        if (rand() % 100 < 5) { // ~5% chance per frame at close range
            Wave whisperWave = ProceduralAudio::generateWhisper(1.5f);
            Sound whisperSnd = LoadSoundFromWave(whisperWave);
            UnloadWave(whisperWave);
            SetSoundVolume(whisperSnd, whisperIntensity);
            PlaySound(whisperSnd);
        }
    }

    // Static sounds when ghost is very close
    if (ghostDistance < 5.0f) {
        float staticIntensity = (1.0f - ghostDistance / 5.0f) * 0.2f;
        if (rand() % 100 < 8) { // ~8% chance per frame at very close range
            Wave staticWave = ProceduralAudio::generateStatic(0.5f);
            Sound staticSnd = LoadSoundFromWave(staticWave);
            UnloadWave(staticWave);
            SetSoundVolume(staticSnd, staticIntensity);
            PlaySound(staticSnd);
        }
    }
}

void AudioManager::setHeartbeatRate(float bpm) {
    heartbeatBPM = std::clamp(bpm, 30.0f, 200.0f);
}

float AudioManager::calculateVolumeFromDistance(Vector3 source, Vector3 listener, float maxDistance) {
    float dx = source.x - listener.x;
    float dy = source.y - listener.y;
    float dz = source.z - listener.z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);

    if (distance >= maxDistance) {
        return 0.0f;
    }
    if (distance <= 0.1f) {
        return 1.0f;
    }

    // Inverse distance falloff
    // Volume = 1 / (1 + distance) normalized so that at maxDistance volume is ~0
    float volume = 1.0f / (1.0f + distance * 0.5f);

    // Additional rolloff near max distance
    float rolloffStart = maxDistance * 0.7f;
    if (distance > rolloffStart) {
        float rolloffFactor = 1.0f - (distance - rolloffStart) / (maxDistance - rolloffStart);
        volume *= rolloffFactor;
    }

    return std::clamp(volume, 0.0f, 1.0f);
}
