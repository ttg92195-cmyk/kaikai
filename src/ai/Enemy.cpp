#include "Enemy.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <random>

static constexpr int MAP_W = 50;
static constexpr int MAP_H = 50;
static constexpr float TILE_SZ = 2.0f;
static constexpr float WAYPOINT_ARRIVE_DIST = 0.5f;
static constexpr float PATH_RECALC_INTERVAL = 0.5f;
static constexpr float CHASE_GIVE_UP_TIME = 6.0f;
static constexpr float SOUND_DETECT_BASE_RANGE = 15.0f;
static constexpr float IDLE_WAIT_TIME_MIN = 2.0f;
static constexpr float IDLE_WAIT_TIME_MAX = 5.0f;

Enemy::Enemy(uint32_t id)
    : id(id)
    , position({25.0f * TILE_SZ, 0.0f, 25.0f * TILE_SZ})
    , targetPos(position)
    , idleTarget(position)
    , rng(std::random_device{}())
{
}

void Enemy::update(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData) {
    animationTime += deltaTime;
    pathTimer += deltaTime;

    // Check if we should start chasing because of a detected sound
    if (soundDetected && !chasing) {
        chasing = true;
        chaseGiveUpTimer = 0.0f;
        targetPos = soundPosition;
        pathTimer = PATH_RECALC_INTERVAL; // force immediate path recalc
        soundDetected = false;
    }

    // Check if we can directly see or hear a player — start chasing
    if (!chasing) {
        for (const auto& player : players) {
            if (player.isDead || player.isSpectator) continue;
            float dist = Vector3Distance(position, player.position);
            // Direct visibility or proximity triggers chase
            float detectRange = SOUND_DETECT_BASE_RANGE;
            if (player.flashlightOn) detectRange *= 1.5f; // flashlight makes player easier to spot
            if (dist < detectRange) {
                // Check line of sight if we have grid data
                if (gridData && !Pathfinding::hasLineOfSight(position, player.position, gridData)) {
                    continue; // wall in the way
                }
                chasing = true;
                chaseGiveUpTimer = 0.0f;
                targetPos = player.position;
                pathTimer = PATH_RECALC_INTERVAL;
                break;
            }
        }
    }

    if (chasing) {
        chase(deltaTime, players, gridData);
    } else {
        idle(deltaTime, gridData);
    }
}

void Enemy::render() const {
    renderModel();
}

void Enemy::detectSound(Vector3 soundPos, float soundRadius) {
    float dist = Vector3Distance(position, soundPos);
    if (dist < SOUND_DETECT_BASE_RANGE + soundRadius) {
        soundDetected = true;
        soundPosition = soundPos;
        this->soundRadius = soundRadius;
    }
}

Vector3 Enemy::getPosition() const {
    return position;
}

bool Enemy::isChasing() const {
    return chasing;
}

uint32_t Enemy::getId() const {
    return id;
}

// ---- Idle (wander) behavior ----

void Enemy::idle(float deltaTime, const uint8_t* gridData) {
    float distToTarget = Vector3Distance(position, idleTarget);

    // Arrived at idle target or haven't picked one yet
    if (distToTarget < WAYPOINT_ARRIVE_DIST || (idleTarget.x == 0.0f && idleTarget.z == 0.0f)) {
        idleTimer += deltaTime;

        // Wait for a random duration before picking a new target
        float waitTime = IDLE_WAIT_TIME_MIN + static_cast<float>(rng() % 1000) / 1000.0f * (IDLE_WAIT_TIME_MAX - IDLE_WAIT_TIME_MIN);
        if (idleTimer < waitTime && distToTarget < WAYPOINT_ARRIVE_DIST) {
            return; // still waiting
        }

        idleTimer = 0.0f;
        if (gridData) {
            pickRandomValidPosition(gridData);
        }
        return;
    }

    // Follow the current idle path
    if (path.empty() || pathTimer >= PATH_RECALC_INTERVAL * 4.0f) {
        if (gridData) {
            Pathfinding pathfinder;
            path = pathfinder.findPath(position, idleTarget, gridData);
            pathTimer = 0.0f;
        }
    }

    // Move along the path
    if (!path.empty()) {
        Vector3 nextWaypoint = path[0];
        float dist = Vector3Distance(position, nextWaypoint);

        while (dist < WAYPOINT_ARRIVE_DIST && !path.empty()) {
            position = nextWaypoint;
            path.erase(path.begin());
            if (path.empty()) break;
            nextWaypoint = path[0];
            dist = Vector3Distance(position, nextWaypoint);
        }

        if (!path.empty() && dist > 0.001f) {
            Vector3 dir = Vector3Scale(Vector3Subtract(nextWaypoint, position), 1.0f / dist);
            float moveAmount = speed * 0.6f * deltaTime; // slower while idle
            if (moveAmount > dist) moveAmount = dist;
            position = Vector3Add(position, Vector3Scale(dir, moveAmount));
        }
    } else {
        // No path — move directly toward idle target (might clip through walls but better than freezing)
        if (distToTarget > 0.001f) {
            Vector3 dir = Vector3Scale(Vector3Subtract(idleTarget, position), 1.0f / distToTarget);
            float moveAmount = speed * 0.6f * deltaTime;
            if (moveAmount > distToTarget) moveAmount = distToTarget;
            position = Vector3Add(position, Vector3Scale(dir, moveAmount));
        }
    }
}

// ---- Chase behavior ----

void Enemy::chase(float deltaTime, const std::vector<PlayerState>& players, const uint8_t* gridData) {
    // Find the nearest alive player to chase
    const PlayerState* nearest = nullptr;
    float nearestDist = FLT_MAX;

    for (const auto& player : players) {
        if (player.isDead || player.isSpectator) continue;
        float dist = Vector3Distance(position, player.position);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = player;
        }
    }

    // If we're chasing a sound and no player is visible, move to sound position
    if (nearest == nullptr) {
        float distToSound = Vector3Distance(position, targetPos);
        if (distToSound < WAYPOINT_ARRIVE_DIST * 2.0f) {
            // Reached the sound source, go back to idle
            chasing = false;
            chaseGiveUpTimer = 0.0f;
            path.clear();
            return;
        }
    } else {
        // Update target to the nearest player
        targetPos = nearest->position;
        chaseGiveUpTimer = 0.0f; // reset give-up timer when we have a target
    }

    // Give up if we've been chasing too long without getting closer
    if (nearest == nullptr) {
        chaseGiveUpTimer += deltaTime;
        if (chaseGiveUpTimer > CHASE_GIVE_UP_TIME) {
            chasing = false;
            chaseGiveUpTimer = 0.0f;
            path.clear();
            return;
        }
    }

    // If the nearest player is too far away, give up
    if (nearest && nearestDist > 25.0f) {
        chaseGiveUpTimer += deltaTime;
        if (chaseGiveUpTimer > CHASE_GIVE_UP_TIME) {
            chasing = false;
            chaseGiveUpTimer = 0.0f;
            path.clear();
            return;
        }
    } else {
        chaseGiveUpTimer = 0.0f;
    }

    // Recalculate path toward target every 0.5 seconds
    if (pathTimer >= PATH_RECALC_INTERVAL && gridData) {
        Pathfinding pathfinder;
        path = pathfinder.findPath(position, targetPos, gridData);
        pathTimer = 0.0f;
    }

    // Move along the path
    if (!path.empty()) {
        Vector3 nextWaypoint = path[0];
        float dist = Vector3Distance(position, nextWaypoint);

        while (dist < WAYPOINT_ARRIVE_DIST && !path.empty()) {
            position = nextWaypoint;
            path.erase(path.begin());
            if (path.empty()) break;
            nextWaypoint = path[0];
            dist = Vector3Distance(position, nextWaypoint);
        }

        if (!path.empty() && dist > 0.001f) {
            Vector3 dir = Vector3Scale(Vector3Subtract(nextWaypoint, position), 1.0f / dist);
            float moveAmount = speed * deltaTime;
            if (moveAmount > dist) moveAmount = dist;
            position = Vector3Add(position, Vector3Scale(dir, moveAmount));
        }
    } else {
        // No path available — move directly toward target
        float distToTarget = Vector3Distance(position, targetPos);
        if (distToTarget > 0.001f) {
            Vector3 dir = Vector3Scale(Vector3Subtract(targetPos, position), 1.0f / distToTarget);
            float moveAmount = speed * deltaTime;
            if (moveAmount > distToTarget) moveAmount = distToTarget;
            position = Vector3Add(position, Vector3Scale(dir, moveAmount));
        }
    }
}

// ---- Rendering ----

void Enemy::renderModel() const {
    // Floating animation offset
    float floatOffset = std::sin(animationTime * 2.0f) * 0.15f;
    float hoverY = floatOffset + 0.1f;

    // Slight side-to-side sway
    float swayAngle = std::sin(animationTime * 1.5f) * 0.05f;

    Vector3 basePos = { position.x, position.y + hoverY, position.z };
    float rotation = getRotation();

    // --- Dark aura / shadow on the ground ---
    Color auraColor = { 10, 0, 20, 80 };
    DrawCircle3D(
        { position.x, position.y + 0.05f, position.z },
        1.8f + std::sin(animationTime * 3.0f) * 0.3f,
        { 0.0f, 0.0f, 0.0f }, 90.0f,
        auraColor
    );

    // --- Semi-transparent dark aura around the body ---
    Color darkAura = { 20, 0, 30, 40 };
    DrawSphere(
        { basePos.x, basePos.y + 1.2f, basePos.z },
        1.6f + std::sin(animationTime * 2.5f) * 0.2f,
        darkAura
    );

    // --- Body: Tall dark cylinder ---
    // Main torso
    Color bodyColor = { 15, 5, 20, 255 };
    DrawCylinder(
        { basePos.x, basePos.y + 0.5f, basePos.z },
        0.35f,  // radius top
        0.45f,  // radius bottom
        2.0f,   // height
        8,      // slices
        bodyColor
    );

    // Tattered lower body / robe effect - wider at bottom
    Color robeColor = { 10, 2, 15, 230 };
    DrawCylinder(
        { basePos.x, basePos.y + 0.1f, basePos.z },
        0.45f,
        0.7f,
        0.6f,
        8,
        robeColor
    );

    // --- Head: Distorted sphere ---
    Color headColor = { 20, 8, 25, 255 };
    float headPulse = 1.0f + std::sin(animationTime * 4.0f) * 0.05f;
    Vector3 headPos = {
        basePos.x + std::sin(animationTime * 1.2f) * 0.03f,
        basePos.y + 2.7f + headPulse * 0.05f,
        basePos.z + std::cos(animationTime * 1.2f) * 0.03f
    };

    // Slightly elongated/distorted head using two overlapping spheres
    DrawSphere(headPos, 0.35f * headPulse, headColor);
    DrawSphere(
        { headPos.x, headPos.y + 0.12f, headPos.z },
        0.25f * headPulse,
        headColor
    );

    // --- Glowing red eyes ---
    float eyeGlow = 0.8f + std::sin(animationTime * 6.0f) * 0.2f;
    Color eyeColor = {
        static_cast<unsigned char>(255 * eyeGlow),
        static_cast<unsigned char>(20 * eyeGlow),
        static_cast<unsigned char>(20 * eyeGlow),
        255
    };

    // Eye glow aura
    Color eyeAuraColor = {
        static_cast<unsigned char>(255 * eyeGlow * 0.4f),
        0, 0, 100
    };

    // Calculate eye positions relative to facing direction
    float eyeForwardX = std::sin(rotation);
    float eyeForwardZ = std::cos(rotation);
    float eyeRightX = std::cos(rotation);
    float eyeRightZ = -std::sin(rotation);

    Vector3 leftEyePos = {
        headPos.x + eyeRightX * 0.12f + eyeForwardX * 0.3f,
        headPos.y + 0.05f,
        headPos.z + eyeRightZ * 0.12f + eyeForwardZ * 0.3f
    };
    Vector3 rightEyePos = {
        headPos.x - eyeRightX * 0.12f + eyeForwardX * 0.3f,
        headPos.y + 0.05f,
        headPos.z - eyeRightZ * 0.12f + eyeForwardZ * 0.3f
    };

    // Eye glow spheres (slightly larger, semi-transparent)
    DrawSphere(leftEyePos, 0.08f, eyeAuraColor);
    DrawSphere(rightEyePos, 0.08f, eyeAuraColor);

    // Actual eye points (small bright spheres)
    DrawSphere(leftEyePos, 0.05f, eyeColor);
    DrawSphere(rightEyePos, 0.05f, eyeColor);

    // --- Long thin arms ---
    Color armColor = { 12, 4, 18, 240 };
    float armSway = std::sin(animationTime * 2.0f) * 0.15f;
    float armDangle = 0.3f + std::sin(animationTime * 1.8f) * 0.1f;

    // Left arm
    Vector3 leftArmBase = {
        basePos.x - 0.4f,
        basePos.y + 2.0f,
        basePos.z
    };
    Vector3 leftArmEnd = {
        leftArmBase.x - 0.3f + armSway,
        leftArmBase.y - 1.8f - armDangle,
        leftArmBase.z - 0.2f
    };

    // Draw arm as a thin cylinder rotated toward the endpoint
    Vector3 leftArmMid = Vector3Scale(Vector3Add(leftArmBase, leftArmEnd), 0.5f);
    float leftArmLen = Vector3Distance(leftArmBase, leftArmEnd);
    DrawCylinderEx(leftArmBase, leftArmEnd, 0.06f, 0.04f, 4, armColor);

    // Left hand — small dark sphere
    DrawSphere(leftArmEnd, 0.08f, armColor);

    // Right arm
    Vector3 rightArmBase = {
        basePos.x + 0.4f,
        basePos.y + 2.0f,
        basePos.z
    };
    Vector3 rightArmEnd = {
        rightArmBase.x + 0.3f - armSway,
        rightArmBase.y - 1.8f - armDangle,
        rightArmBase.z - 0.2f
    };

    DrawCylinderEx(rightArmBase, rightArmEnd, 0.06f, 0.04f, 4, armColor);

    // Right hand — small dark sphere with claw-like fingers
    DrawSphere(rightArmEnd, 0.08f, armColor);
    // Tiny finger cylinders
    for (int f = 0; f < 3; ++f) {
        float fAngle = -0.3f + f * 0.3f;
        Vector3 fingerEnd = {
            rightArmEnd.x + std::sin(fAngle) * 0.15f,
            rightArmEnd.y - 0.18f,
            rightArmEnd.z + std::cos(fAngle) * 0.1f
        };
        DrawCylinderEx(rightArmEnd, fingerEnd, 0.02f, 0.01f, 3, armColor);
    }

    // --- Chase mode visual: red pulsing outline when chasing ---
    if (chasing) {
        float pulse = 0.5f + std::sin(animationTime * 8.0f) * 0.5f;
        Color chaseAura = {
            static_cast<unsigned char>(80 * pulse),
            0, 0,
            static_cast<unsigned char>(60 * pulse)
        };
        DrawSphere(
            { basePos.x, basePos.y + 1.2f, basePos.z },
            1.2f + pulse * 0.3f,
            chaseAura
        );
    }

    // --- Wispy particle-like effects around the ghost ---
    for (int i = 0; i < 4; ++i) {
        float pTime = animationTime * (0.8f + i * 0.3f) + i * 1.57f;
        float px = basePos.x + std::sin(pTime) * (0.8f + i * 0.2f);
        float py = basePos.y + 1.0f + std::sin(pTime * 0.7f) * 0.5f + i * 0.4f;
        float pz = basePos.z + std::cos(pTime * 1.3f) * (0.8f + i * 0.15f);
        Color wispColor = { 30, 10, 40, static_cast<unsigned char>(100 - i * 20) };
        DrawSphere({ px, py, pz }, 0.08f + i * 0.02f, wispColor);
    }
}

// ---- Helpers ----

bool Enemy::pickRandomValidPosition(const uint8_t* gridData) {
    if (!gridData) return false;

    // Try random positions until we find a valid (non-wall) cell
    std::uniform_int_distribution<int> distX(1, MAP_W - 2);
    std::uniform_int_distribution<int> distZ(1, MAP_H - 2);

    for (int attempt = 0; attempt < 50; ++attempt) {
        int gx = distX(rng);
        int gz = distZ(rng);
        if (Pathfinding::isValidCell(gx, gz, gridData)) {
            idleTarget = Pathfinding::gridToWorld(gx, gz);
            path.clear(); // clear old path so a new one is computed
            return true;
        }
    }

    // Fallback: stay near current position
    idleTarget = position;
    return false;
}

float Enemy::getRotation() const {
    // Calculate rotation from position to targetPos
    Vector3 dir = Vector3Subtract(targetPos, position);
    if (Vector3Length(dir) < 0.001f) return 0.0f;
    return std::atan2(dir.x, dir.z);
}
