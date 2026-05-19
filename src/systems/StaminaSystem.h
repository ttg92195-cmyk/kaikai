#pragma once
#include "../game/Player.h"

class StaminaSystem {
public:
    StaminaSystem() = default;
    void update(float deltaTime, PlayerState& player);
    
    void drain(float amount); // called when running
    void regenerate(float amount);
    
    bool canRun(const PlayerState& player) const;
    float getStaminaPercentage(const PlayerState& player) const;
    
    // Exhaustion effects
    bool isExhausted(const PlayerState& player) const;
    float getMovementSpeedMultiplier(const PlayerState& player) const;
    
private:
    static constexpr float EXHAUSTED_THRESHOLD = 10.0f;
    static constexpr float RECOVERY_THRESHOLD = 30.0f;
    bool wasExhausted = false;
};
