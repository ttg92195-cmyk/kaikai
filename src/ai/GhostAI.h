#pragma once
#if defined(KAIKAI_HEADLESS)
#include "../utils/HeadlessCompat.h"
#else
#include "raylib.h"
#include "raymath.h"
#endif
#include "Pathfinding.h"
#include "../networking/PacketTypes.h"
#include <vector>
#include <cstdint>

enum class GhostState {
    PATROL,        // wandering around the map
    INVESTIGATE,   // going to a sound source
    CHASE,         // actively chasing a player
    AMBUSH,        // hiding and waiting
    RETREAT        // backing off after a catch
};

struct SoundEvent {
    Vector3 position;
    float radius; // how loud
    float timestamp;
    uint32_t sourcePlayerId;
};

class GhostAI {
public:
    GhostAI();
    ~GhostAI() = default;
    
    void update(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData);
    void notifySound(const SoundEvent& sound);
    
    // AI state
    GhostState getState() const;
    Vector3 getPosition() const;
    Vector3 getTargetPosition() const;
    float getSpeed() const;
    
    // Configuration
    void setPatrolPoints(const std::vector<Vector3>& points);
    void setHearRange(float range);
    void setChaseSpeed(float speed);
    void setPatrolSpeed(float speed);
    
private:
    Vector3 position = {0, 0, 0};
    Vector3 targetPosition = {0, 0, 0};
    GhostState state = GhostState::PATROL;
    std::vector<Vector3> currentPath;
    std::vector<Vector3> patrolPoints;
    std::vector<SoundEvent> pendingSounds;
    size_t currentPatrolIndex = 0;
    float pathUpdateTimer = 0.0f;
    float stateTimer = 0.0f;
    float hearRange = 15.0f;
    float chaseSpeed = 4.5f;
    float patrolSpeed = 2.0f;
    uint32_t chaseTargetId = 0;
    float retreatTimer = 0.0f;
    Vector3 ambushPosition = {0, 0, 0};
    float ambushTimer = 0.0f;
    
    // State handlers
    void updatePatrol(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData);
    void updateInvestigate(float deltaTime, const uint8_t* gridData);
    void updateChase(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData);
    void updateAmbush(float deltaTime, const std::vector<PlayerState>& players);
    void updateRetreat(float deltaTime);
    
    // Helpers
    void followPath(float deltaTime, float speed);
    void recalculatePath(Vector3 target, const uint8_t* gridData);
    const PlayerState* findNearestPlayer(const std::vector<PlayerState>& players);
    void processSounds();
    bool canSeePlayer(const PlayerState& player, const uint8_t* gridData);
};
