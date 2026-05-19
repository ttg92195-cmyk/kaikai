#include "Ghost.h"
#if !defined(KAIKAI_HEADLESS)
#include "raymath.h"
#endif
#include <cmath>
#include <algorithm>
#include <limits>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Ghost::Ghost(uint32_t id) {
    state.id = id;
    state.position = {0.0f, 0.0f, 0.0f};
    state.rotation = 0.0f;
    state.stamina = 100.0f;
    state.sanity = 100.0f;
    state.battery = 100.0f;
    state.isGhost = true;
    state.isDead = false;
    state.isSpectator = false;
    state.flashlightOn = false;
    state.isRunning = false;
    state.headBob = 0.0f;
    state.footstepTimer = 0.0f;
    catchCooldown = 0.0f;

#if !defined(KAIKAI_HEADLESS)
    // Ghost camera – wider FOV for night-vision aesthetic
    camera.position = state.position;
    camera.target = {0.0f, 0.0f, -1.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = GHOST_FOV;
    camera.projection = CAMERA_PERSPECTIVE;
#endif
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const PlayerState& Ghost::getState() const { return state; }
PlayerState& Ghost::getStateMut() { return state; }
float Ghost::getCatchCooldown() const { return catchCooldown; }

bool Ghost::canCatch() const {
    return catchCooldown <= 0.0f;
}

// ---------------------------------------------------------------------------
// Main update
// ---------------------------------------------------------------------------

void Ghost::update(float deltaTime, const std::vector<PlayerState>& survivors) {
    // Cooldown tick
    if (catchCooldown > 0.0f) {
        catchCooldown -= deltaTime;
        if (catchCooldown < 0.0f) catchCooldown = 0.0f;
    }

#if !defined(KAIKAI_HEADLESS)
    // --- Ghost input (ghost player controls directly) ---
    Vector2 mouseDelta = GetMouseDelta();
    rotate(mouseDelta.x);

    // Pitch
    pitch -= mouseDelta.y * 0.003f;
    if (pitch > 1.5f)  pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;

    // Ghost movement – no collision with walls (no-clip)
    if (IsKeyDown(KEY_W)) moveForward(deltaTime);
    if (IsKeyDown(KEY_S)) moveBackward(deltaTime);
    if (IsKeyDown(KEY_A)) moveLeft(deltaTime);
    if (IsKeyDown(KEY_D)) moveRight(deltaTime);

    // Ghost can hold Shift to sprint faster
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        // Already at GHOST_SPEED; shift gives a boost
        // (handled via speed multiplier in move functions)
    }

    // Catch key – press E to attempt catch
    if (IsKeyPressed(KEY_E) && canCatch()) {
        uint32_t nearestId = findNearestSurvivor(survivors);
        if (nearestId != 0) {
            catchPlayer(nearestId);
        }
    }
#endif

    // --- Night vision: ghost always has full visibility ---
    // No battery drain, no flashlight needed
    state.battery = 100.0f;
    state.flashlightOn = false;

#if !defined(KAIKAI_HEADLESS)
    updateCamera();
#endif
}

// ---------------------------------------------------------------------------
// Possess / activate ghost mode
// ---------------------------------------------------------------------------

void Ghost::possess() {
    state.isGhost = true;
    state.isDead = false;
    state.isSpectator = false;
    state.stamina = 100.0f;
    state.sanity = 100.0f;
    state.battery = 100.0f;
    catchCooldown = 0.0f;
}

// ---------------------------------------------------------------------------
// Catch mechanic
// ---------------------------------------------------------------------------

void Ghost::catchPlayer(uint32_t targetId) {
    if (!canCatch()) return;
    catchCooldown = CATCH_COOLDOWN;

    // The actual kill is handled by the Game class which listens for
    // the catch event. Here we just trigger the cooldown and mark the
    // interaction. Game::playerInteract or Game::handleGhostCatch will
    // call the target player's takeDamage / die.
    // We set a flag in state for the network to pick up:
    // (In a full implementation this would dispatch a network event.)
    (void)targetId; // Used by Game layer
}

bool Ghost::isInCatchRange(const PlayerState& survivor) const {
    if (survivor.isDead || survivor.isSpectator) return false;

    float dx = state.position.x - survivor.position.x;
    float dz = state.position.z - survivor.position.z;
    float distSq = dx * dx + dz * dz;

    return distSq <= (CATCH_RANGE * CATCH_RANGE);
}

uint32_t Ghost::findNearestSurvivor(const std::vector<PlayerState>& survivors) const {
    uint32_t nearestId = 0;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (const auto& s : survivors) {
        if (s.isDead || s.isSpectator || s.isGhost) continue;

        float dx = state.position.x - s.position.x;
        float dz = state.position.z - s.position.z;
        float distSq = dx * dx + dz * dz;

        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearestId = s.id;
        }
    }

    // Only return if within catch range
    if (nearestDistSq <= (CATCH_RANGE * CATCH_RANGE)) {
        return nearestId;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Ghost movement (no-clip through walls)
// ---------------------------------------------------------------------------

void Ghost::moveForward(float deltaTime) {
    float speed = GHOST_SPEED;
#if !defined(KAIKAI_HEADLESS)
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed = GHOST_SPEED * 1.5f; // ghost sprint

    float yaw = state.rotation;
    Vector3 forward = {sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)};
#else
    float yaw = state.rotation;
    Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
#endif

    state.position.x += forward.x * speed * deltaTime;
    state.position.y += forward.y * speed * deltaTime;
    state.position.z += forward.z * speed * deltaTime;

    // Clamp Y so ghost doesn't fly out of the map
    if (state.position.y < 0.0f) state.position.y = 0.0f;
    if (state.position.y > 3.0f) state.position.y = 3.0f;
}

void Ghost::moveBackward(float deltaTime) {
    float speed = GHOST_SPEED;
#if !defined(KAIKAI_HEADLESS)
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed = GHOST_SPEED * 1.5f;

    float yaw = state.rotation;
    Vector3 forward = {sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)};
#else
    float yaw = state.rotation;
    Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
#endif

    state.position.x -= forward.x * speed * deltaTime;
    state.position.y -= forward.y * speed * deltaTime;
    state.position.z -= forward.z * speed * deltaTime;

    if (state.position.y < 0.0f) state.position.y = 0.0f;
    if (state.position.y > 3.0f) state.position.y = 3.0f;
}

void Ghost::moveLeft(float deltaTime) {
    float speed = GHOST_SPEED;
#if !defined(KAIKAI_HEADLESS)
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed = GHOST_SPEED * 1.5f;
#endif

    float yaw = state.rotation;
    Vector3 right = {cosf(yaw), 0.0f, -sinf(yaw)};

    state.position.x -= right.x * speed * deltaTime;
    state.position.z -= right.z * speed * deltaTime;
}

void Ghost::moveRight(float deltaTime) {
    float speed = GHOST_SPEED;
#if !defined(KAIKAI_HEADLESS)
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed = GHOST_SPEED * 1.5f;
#endif

    float yaw = state.rotation;
    Vector3 right = {cosf(yaw), 0.0f, -sinf(yaw)};

    state.position.x += right.x * speed * deltaTime;
    state.position.z += right.z * speed * deltaTime;
}

void Ghost::rotate(float deltaX) {
    float sensitivity = 0.003f;
    state.rotation += deltaX * sensitivity;
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

#if !defined(KAIKAI_HEADLESS)
void Ghost::updateCamera() {
    float eyeHeight = GHOST_EYE_HEIGHT + state.position.y;

    camera.position = {
        state.position.x,
        eyeHeight,
        state.position.z
    };

    float yaw = state.rotation;
    float lookDirX = sinf(yaw) * cosf(pitch);
    float lookDirY = sinf(pitch);
    float lookDirZ = cosf(yaw) * cosf(pitch);

    camera.target = {
        camera.position.x + lookDirX,
        camera.position.y + lookDirY,
        camera.position.z + lookDirZ
    };
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = GHOST_FOV;
    camera.projection = CAMERA_PERSPECTIVE;
}
#endif // !KAIKAI_HEADLESS
