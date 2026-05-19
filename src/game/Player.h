#pragma once

#include "../networking/PacketTypes.h"
#include "../utils/Constants.h"
#include "raylib.h"
#include <cstdint>

// PlayerState is defined in networking/PacketTypes.h (canonical definition).
// This header extends it with the Player class used by the game simulation.

class Player {
public:
    explicit Player(uint32_t id);
    ~Player() = default;

    void handleInput(float deltaTime);
    void update(float deltaTime, const uint8_t* mapData = nullptr);

    const PlayerState& getState() const;
    PlayerState& getStateMut();
    uint32_t getId() const;
    Camera3D getCamera() const;

    // Movement
    void moveForward(float deltaTime);
    void moveBackward(float deltaTime);
    void moveLeft(float deltaTime);
    void moveRight(float deltaTime);
    void rotate(float deltaX);
    void toggleRun(bool running);
    void toggleFlashlight();

    // Collision
    bool checkCollision(Vector3 newPos, const uint8_t* mapData);

    // Damage / Death / Respawn
    void takeDamage(float amount);
    void die();
    void respawn(Vector3 pos);

    // PLAYER_HEIGHT and PLAYER_RADIUS are in Kaikai namespace (Constants.h)
    // and also accessible here for convenience
    static constexpr float PLAYER_HEIGHT_VAL = Kaikai::PLAYER_HEIGHT;
    static constexpr float PLAYER_RADIUS_VAL = Kaikai::PLAYER_RADIUS;

private:
    PlayerState state;
    bool isRunning = false;
    Vector3 moveDirection = { 0.0f, 0.0f, 0.0f };

    // Camera
    Camera3D camera;
    float headBobAmplitude = 0.0f;
    float headBobPhase     = 0.0f;
    float pitch            = 0.0f;   // Vertical look angle
    void updateCamera();

    // Map constants for collision (from Kaikai namespace)
    using Kaikai::MAP_WIDTH;
    using Kaikai::MAP_HEIGHT;
    using Kaikai::TILE_SIZE;

    // Stamina thresholds
    static constexpr float STAMINA_MIN_TO_RUN = 10.0f;
    static constexpr float STAMINA_DRAIN_RATE = 10.0f;
    static constexpr float STAMINA_REGEN_RATE = 5.0f;
    static constexpr float BATTERY_DRAIN_RATE = 3.0f;

    // Head bob
    static constexpr float HEAD_BOB_WALK_SPEED = 8.0f;
    static constexpr float HEAD_BOB_RUN_SPEED  = 12.0f;
    static constexpr float HEAD_BOB_WALK_AMP   = 0.04f;
    static constexpr float HEAD_BOB_RUN_AMP    = 0.08f;

    // Footsteps
    static constexpr float FOOTSTEP_WALK_INTERVAL = 0.5f;
    static constexpr float FOOTSTEP_RUN_INTERVAL  = 0.3f;
};
