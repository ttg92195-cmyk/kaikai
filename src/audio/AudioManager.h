#pragma once
#include "raylib.h"
#include "../networking/PacketTypes.h"
#include <unordered_map>
#include <string>
#include <vector>

class AudioManager {
public:
    AudioManager();
    ~AudioManager();
    
    void init();
    void shutdown();
    void update(float deltaTime);
    
    // Sound playback
    void playSound(const std::string& name, float volume = 1.0f, float pitch = 1.0f);
    void playSound3D(const std::string& name, Vector3 position, Vector3 listenerPos, float maxDistance = 30.0f);
    void playMusic(const std::string& name, float volume = 0.5f);
    void stopMusic();
    
    // Proximity-based audio
    void updateProximityAudio(const std::vector<std::pair<uint32_t, Vector3>>& otherPlayers, Vector3 localPos);
    
    // Ambient
    void playAmbient(); // horror ambient sounds
    void updateAmbient(float sanity); // change based on sanity
    
    // Footsteps
    void playFootstep(Vector3 position, bool running);
    void updateFootstepSounds(float deltaTime, const PlayerState& player);
    
    // Ghost proximity
    void updateGhostProximity(float ghostDistance); // heartbeat + ghost sounds
    void setHeartbeatRate(float bpm); // change heartbeat speed
    
private:
    bool initialized = false;
    float heartbeatBPM = 60.0f;
    float heartbeatTimer = 0.0f;
    float footstepTimer = 0.0f;
    float ambientTimer = 0.0f;
    Sound heartbeatSound;
    Sound footstepSound;
    Sound doorSound;
    Sound ambientSound;
    Music backgroundMusic;
    
    // Proximity voice (placeholder for actual voice data)
    std::unordered_map<uint32_t, float> playerVolumes;
    
    float calculateVolumeFromDistance(Vector3 source, Vector3 listener, float maxDistance);
};
