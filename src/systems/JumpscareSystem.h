#pragma once
#include "raylib.h"
#include <cstdint>
#include <vector>

struct JumpscareEvent {
    uint32_t targetPlayerId;
    float duration;
    float timer;
    bool active;
    Vector3 ghostPosition; // where the ghost appears
    float intensity; // how scary
};

class JumpscareSystem {
public:
    JumpscareSystem() = default;
    void update(float deltaTime);
    
    // Trigger a jumpscare
    void triggerJumpscare(uint32_t playerId, Vector3 ghostPos, float intensity = 1.0f);
    
    // Rendering
    void render(const Camera3D& camera) const;
    void renderFlashEffect() const; // screen flash
    bool isActive() const;
    
    // Dynamic trigger conditions
    bool checkDynamicTrigger(const PlayerState& player, float deltaTime,
                             bool aloneInRoom, float sanity);
    
private:
    std::vector<JumpscareEvent> activeJumpscares;
    float cooldownTimer = 0.0f;
    float aloneTimer = 0.0f;
    static constexpr float MIN_COOLDOWN = 30.0f; // minimum seconds between jumpscares
    static constexpr float ALONE_TRIGGER_TIME = 15.0f; // alone for this long triggers
    float screenFlashAlpha = 0.0f;
};
