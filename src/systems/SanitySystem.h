#pragma once
#include "../game/Player.h"

class SanitySystem {
public:
    SanitySystem() = default;
    void update(float deltaTime, PlayerState& player, bool inDarkArea, float ghostDistance);
    
    float getSanityPercentage(const PlayerState& player) const;
    
    // Effects based on sanity level
    float getScreenShakeIntensity(const PlayerState& player) const;
    float getVignetteIntensity(const PlayerState& player) const;
    float getColorDistortion(const PlayerState& player) const;
    float getAudioDistortion(const PlayerState& player) const;
    
    // Triggers
    bool shouldTriggerJumpscare(const PlayerState& player) const; // based on sanity threshold
    bool isHallucinating(const PlayerState& player) const;
    
private:
    float hallucinationTimer = 0.0f;
    float nextHallucinationTime = 30.0f;
    static constexpr float JUMPSCARE_SANITY_THRESHOLD = 20.0f;
    static constexpr float HALLUCINATION_THRESHOLD = 40.0f;
};
