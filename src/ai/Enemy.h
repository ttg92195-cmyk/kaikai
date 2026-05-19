#pragma once
#include "raylib.h"
#include "Pathfinding.h"
#include <vector>
#include <cstdint>
#include <random>

class Enemy {
public:
    Enemy(uint32_t id);
    ~Enemy() = default;
    
    void update(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData);
    void render() const; // render the enemy model
    
    // Sound detection
    void detectSound(Vector3 soundPos, float soundRadius);
    
    // State
    Vector3 getPosition() const;
    bool isChasing() const;
    uint32_t getId() const;
    
private:
    uint32_t id;
    Vector3 position;
    Vector3 targetPos;
    std::vector<Vector3> path;
    bool chasing = false;
    float speed = 3.5f;
    float pathTimer = 0.0f;
    float idleTimer = 0.0f;
    Vector3 idleTarget;
    std::mt19937 rng;
    bool soundDetected = false;
    Vector3 soundPosition;
    float soundRadius = 0.0f;
    float chaseGiveUpTimer = 0.0f;
    float animationTime = 0.0f;
    
    void idle(float deltaTime, const uint8_t* gridData);
    void chase(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData);
    void renderModel() const; // procedural 3D model using raylib primitives
    
    // Helpers
    bool pickRandomValidPosition(const uint8_t* gridData);
    float getRotation() const;
};
