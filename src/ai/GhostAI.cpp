#include "GhostAI.h"
#if defined(KAIKAI_HEADLESS)
#include "../utils/HeadlessCompat.h"
#endif
#include <algorithm>
#include <cmath>
#include <cfloat>
#if defined(KAIKAI_HEADLESS)
#include <chrono>
#endif

static constexpr float CATCH_DISTANCE = 2.0f;
static constexpr float CHASE_GIVE_UP_DISTANCE = 20.0f;
static constexpr float PATH_RECALC_INTERVAL = 0.5f;
static constexpr float RETREAT_DURATION = 3.0f;
static constexpr float AMBUSH_TIMEOUT = 10.0f;
static constexpr float AMBUSH_DETECT_RADIUS = 8.0f;
static constexpr float INVESTIGATE_ARRIVE_DIST = 1.5f;
static constexpr float WAYPOINT_ARRIVE_DIST = 0.5f;
static constexpr int MAP_W = 50;
static constexpr int MAP_H = 50;

GhostAI::GhostAI() {
    position = {25.0f * 2.0f, 0.0f, 25.0f * 2.0f}; // center of map
    targetPosition = position;
}

void GhostAI::update(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData) {
    stateTimer += deltaTime;
    pathUpdateTimer += deltaTime;

    // Process any pending sound events regardless of state
    processSounds();

    switch (state) {
        case GhostState::PATROL:
            updatePatrol(deltaTime, players, gridData);
            break;
        case GhostState::INVESTIGATE:
            updateInvestigate(deltaTime, gridData);
            break;
        case GhostState::CHASE:
            updateChase(deltaTime, players, gridData);
            break;
        case GhostState::AMBUSH:
            updateAmbush(deltaTime, players);
            break;
        case GhostState::RETREAT:
            updateRetreat(deltaTime);
            break;
    }
}

void GhostAI::notifySound(const SoundEvent& sound) {
    pendingSounds.push_back(sound);
}

GhostState GhostAI::getState() const {
    return state;
}

Vector3 GhostAI::getPosition() const {
    return position;
}

Vector3 GhostAI::getTargetPosition() const {
    return targetPosition;
}

float GhostAI::getSpeed() const {
    switch (state) {
        case GhostState::CHASE:    return chaseSpeed;
        case GhostState::PATROL:   return patrolSpeed;
        case GhostState::INVESTIGATE: return patrolSpeed * 1.3f;
        case GhostState::AMBUSH:   return 0.0f;
        case GhostState::RETREAT:  return chaseSpeed * 0.7f;
        default: return patrolSpeed;
    }
}

void GhostAI::setPatrolPoints(const std::vector<Vector3>& points) {
    patrolPoints = points;
    currentPatrolIndex = 0;
}

void GhostAI::setHearRange(float range) {
    hearRange = range;
}

void GhostAI::setChaseSpeed(float speed) {
    chaseSpeed = speed;
}

void GhostAI::setPatrolSpeed(float speed) {
    patrolSpeed = speed;
}

// ---- State Handlers ----

void GhostAI::updatePatrol(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData) {
    // Check if any player is visible — immediately switch to CHASE
    for (const auto& player : players) {
        if (player.isDead || player.isSpectator) continue;
        if (canSeePlayer(player, gridData)) {
            chaseTargetId = player.id;
            state = GhostState::CHASE;
            stateTimer = 0.0f;
            pathUpdateTimer = PATH_RECALC_INTERVAL; // force immediate recalc
            return;
        }
    }

    // Check if a sound event was processed — switch to INVESTIGATE
    if (!pendingSounds.empty()) {
        // Pick the most important sound (most recent / loudest)
        SoundEvent best = pendingSounds[0];
        for (const auto& s : pendingSounds) {
            float scoreS = s.radius + (1.0f / (1.0f + s.timestamp));
            float scoreBest = best.radius + (1.0f / (1.0f + best.timestamp));
            if (scoreS > scoreBest) best = s;
        }

        // Check if the sound is within hearing range
        float dist = Vector3Distance(position, best.position);
        if (dist <= hearRange + best.radius) {
            targetPosition = best.position;
            state = GhostState::INVESTIGATE;
            stateTimer = 0.0f;
            pathUpdateTimer = PATH_RECALC_INTERVAL;
            pendingSounds.clear();
            return;
        }
    }

    // No threats detected — continue patrol
    if (patrolPoints.empty()) {
        // No patrol points set, just idle in place
        return;
    }

    // Move toward the current patrol point
    Vector3 patrolTarget = patrolPoints[currentPatrolIndex];
    float distToTarget = Vector3Distance(position, patrolTarget);

    if (distToTarget < WAYPOINT_ARRIVE_DIST) {
        // Arrived at patrol point, advance to next
        currentPatrolIndex = (currentPatrolIndex + 1) % patrolPoints.size();
        patrolTarget = patrolPoints[currentPatrolIndex];
    }

    // Recalculate path periodically or when we don't have one
    if (currentPath.empty() || pathUpdateTimer >= PATH_RECALC_INTERVAL * 4.0f) {
        recalculatePath(patrolTarget, gridData);
        pathUpdateTimer = 0.0f;
    }

    followPath(deltaTime, patrolSpeed);
}

void GhostAI::updateInvestigate(float deltaTime, const uint8_t* gridData) {
    float distToTarget = Vector3Distance(position, targetPosition);

    // Arrived at the sound source
    if (distToTarget < INVESTIGATE_ARRIVE_DIST) {
        // Nothing found — go back to patrol
        state = GhostState::PATROL;
        stateTimer = 0.0f;
        currentPath.clear();
        return;
    }

    // Timeout: if we've been investigating too long, give up
    if (stateTimer > 8.0f) {
        state = GhostState::PATROL;
        stateTimer = 0.0f;
        currentPath.clear();
        return;
    }

    // Recalculate path to the sound source
    if (currentPath.empty() || pathUpdateTimer >= PATH_RECALC_INTERVAL * 2.0f) {
        recalculatePath(targetPosition, gridData);
        pathUpdateTimer = 0.0f;
    }

    followPath(deltaTime, patrolSpeed * 1.3f);
}

void GhostAI::updateChase(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData) {
    // Find the chase target
    const PlayerState* target = nullptr;
    for (const auto& player : players) {
        if (player.id == chaseTargetId) {
            target = &player;
            break;
        }
    }

    // If the target is gone/dead/spectating, go back to patrol
    if (!target || target->isDead || target->isSpectator) {
        state = GhostState::PATROL;
        stateTimer = 0.0f;
        currentPath.clear();
        return;
    }

    float distToTarget = Vector3Distance(position, target->position);

    // Player caught!
    if (distToTarget < CATCH_DISTANCE) {
        // Switch to RETREAT after a successful catch
        state = GhostState::RETREAT;
        stateTimer = 0.0f;
        retreatTimer = 0.0f;
        currentPath.clear();

        // Set retreat target: move away from current position in the opposite direction
        Vector3 dir = Vector3Subtract(position, target->position);
        float len = Vector3Length(dir);
        if (len > 0.001f) {
            dir = Vector3Scale(dir, 1.0f / len);
        } else {
            dir = {1.0f, 0.0f, 0.0f};
        }
        targetPosition = Vector3Add(position, Vector3Scale(dir, 10.0f));
        return;
    }

    // Player escaped — give up chase
    if (distToTarget > CHASE_GIVE_UP_DISTANCE) {
        // But only if we can't see them
        if (!canSeePlayer(*target, gridData)) {
            state = GhostState::PATROL;
            stateTimer = 0.0f;
            currentPath.clear();
            return;
        }
    }

    // Check if we should switch to a closer player
    const PlayerState* nearest = findNearestPlayer(players);
    if (nearest && nearest->id != chaseTargetId) {
        float distToNearest = Vector3Distance(position, nearest->position);
        // Switch targets if another player is significantly closer
        if (distToNearest < distToTarget * 0.6f) {
            chaseTargetId = nearest->id;
            target = nearest;
        }
    }

    // Recalculate path to target periodically
    if (pathUpdateTimer >= PATH_RECALC_INTERVAL) {
        recalculatePath(target->position, gridData);
        pathUpdateTimer = 0.0f;
    }

    followPath(deltaTime, chaseSpeed);
}

void GhostAI::updateAmbush(float deltaTime, const std::vector<PlayerState>& players) {
    // Check if a player has come within ambush detection radius
    for (const auto& player : players) {
        if (player.isDead || player.isSpectator) continue;
        float dist = Vector3Distance(position, player.position);
        if (dist < AMBUSH_DETECT_RADIUS) {
            // Spring the ambush — chase the player!
            chaseTargetId = player.id;
            state = GhostState::CHASE;
            stateTimer = 0.0f;
            pathUpdateTimer = PATH_RECALC_INTERVAL;
            currentPath.clear();
            return;
        }
    }

    // Timeout — give up and go back to patrol
    ambushTimer += deltaTime;
    if (ambushTimer >= AMBUSH_TIMEOUT) {
        state = GhostState::PATROL;
        stateTimer = 0.0f;
        currentPath.clear();
    }

    // Ghost stays stationary during ambush — no movement
}

void GhostAI::updateRetreat(float deltaTime) {
    retreatTimer += deltaTime;

    // After retreat duration, go back to patrol
    if (retreatTimer >= RETREAT_DURATION) {
        state = GhostState::PATROL;
        stateTimer = 0.0f;
        retreatTimer = 0.0f;
        currentPath.clear();
        return;
    }

    // Move toward the retreat target
    float dist = Vector3Distance(position, targetPosition);
    if (dist > WAYPOINT_ARRIVE_DIST) {
        Vector3 dir = Vector3Subtract(targetPosition, position);
        float len = Vector3Length(dir);
        if (len > 0.001f) {
            dir = Vector3Scale(dir, 1.0f / len);
            float moveSpeed = chaseSpeed * 0.7f;
            position = Vector3Add(position, Vector3Scale(dir, moveSpeed * deltaTime));
        }
    }
}

// ---- Helpers ----

void GhostAI::followPath(float deltaTime, float speed) {
    if (currentPath.empty()) return;

    // Move toward the next waypoint in the path
    Vector3 nextWaypoint = currentPath[0];
    Vector3 toWaypoint = Vector3Subtract(nextWaypoint, position);
    float dist = Vector3Length(toWaypoint);

    // If we've arrived at the current waypoint, advance
    while (dist < WAYPOINT_ARRIVE_DIST && !currentPath.empty()) {
        position = nextWaypoint;
        currentPath.erase(currentPath.begin());
        if (currentPath.empty()) return;
        nextWaypoint = currentPath[0];
        toWaypoint = Vector3Subtract(nextWaypoint, position);
        dist = Vector3Length(toWaypoint);
    }

    if (dist > 0.001f) {
        Vector3 dir = Vector3Scale(toWaypoint, 1.0f / dist);
        float moveAmount = speed * deltaTime;
        if (moveAmount > dist) moveAmount = dist;
        position = Vector3Add(position, Vector3Scale(dir, moveAmount));

        // Update rotation to face movement direction
        targetPosition = nextWaypoint;
    }
}

void GhostAI::recalculatePath(Vector3 target, const uint8_t* gridData) {
    if (!gridData) return;

    Pathfinding pathfinder;
    currentPath = pathfinder.findPath(position, target, gridData);

    // Remove the first waypoint if it's very close to our current position
    while (!currentPath.empty() && Vector3Distance(position, currentPath[0]) < 0.3f) {
        currentPath.erase(currentPath.begin());
    }
}

const PlayerState* GhostAI::findNearestPlayer(const std::vector<PlayerState>& players) {
    const PlayerState* nearest = nullptr;
    float nearestDist = FLT_MAX;

    for (const auto& player : players) {
        if (player.isDead || player.isSpectator) continue;
        float dist = Vector3Distance(position, player.position);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = &player;
        }
    }

    return nearest;
}

void GhostAI::processSounds() {
    // Remove expired sounds (older than 10 seconds game time)
#if defined(KAIKAI_HEADLESS)
    // In headless mode, use a simple steady_clock based time
    static auto startChrono = std::chrono::steady_clock::now();
    float currentTime = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - startChrono).count();
#else
    float currentTime = static_cast<float>(GetTime());
#endif
    pendingSounds.erase(
        std::remove_if(pendingSounds.begin(), pendingSounds.end(),
            [currentTime](const SoundEvent& s) {
                return (currentTime - s.timestamp) > 10.0f;
            }),
        pendingSounds.end());
}

bool GhostAI::canSeePlayer(const PlayerState& player, const uint8_t* gridData) {
    if (player.isDead || player.isSpectator) return false;

    float dist = Vector3Distance(position, player.position);
    if (dist > hearRange * 1.5f) return false; // sight range slightly beyond hear range

    // Flashlight on makes player more visible from farther away
    if (player.flashlightOn && dist > hearRange * 2.5f) return false;
    if (!player.flashlightOn && dist > hearRange * 1.2f) return false;

    // Line of sight check through grid cells
    return Pathfinding::hasLineOfSight(position, player.position, gridData);
}
