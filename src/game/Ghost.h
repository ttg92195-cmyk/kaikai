#pragma once
#include "Player.h"
#include <cstdint>
#include <vector>

class Ghost {
public:
    Ghost(uint32_t id);
    ~Ghost() = default;

    void update(float deltaTime, const std::vector<PlayerState>& survivors);
    void possess();   // activate ghost mode
    void catchPlayer(uint32_t targetId);
    bool isInCatchRange(const PlayerState& survivor) const;

    const PlayerState& getState() const;
    PlayerState& getStateMut();

    // Movement – ghost can walk through walls
    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void moveLeft(float deltaTime);
    void moveRight(float deltaTime);
    void rotate(float deltaX);

    // Find the nearest living survivor
    uint32_t findNearestSurvivor(const std::vector<PlayerState>& survivors) const;

    float getCatchCooldown() const;
    bool canCatch() const;

private:
    PlayerState state;
    float catchCooldown = 0.0f;

    // Camera for first-person ghost view
#if !defined(KAIKAI_HEADLESS)
    Camera3D camera;
    float pitch = 0.0f;
    void updateCamera();
#endif

    static constexpr float CATCH_RANGE = 2.0f;
    static constexpr float CATCH_COOLDOWN = 3.0f;
    static constexpr float GHOST_SPEED = 4.5f;
    static constexpr float GHOST_FOV = 90.0f;       // wider FOV for night vision feel
    static constexpr float AUTOCHASE_SPEED = 3.5f;   // speed when auto-chasing nearest
    static constexpr float GHOST_EYE_HEIGHT = 1.7f;

    // Map constants
    static constexpr int MAP_WIDTH = 50;
    static constexpr int MAP_HEIGHT = 50;
    static constexpr float TILE_SIZE = 2.0f;
};
