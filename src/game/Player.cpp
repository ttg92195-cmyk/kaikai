#include "Player.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Player::Player(uint32_t id) {
    state.id = id;
    state.position = {0.0f, 0.0f, 0.0f};
    state.rotation = 0.0f;
    state.stamina = 100.0f;
    state.sanity = 100.0f;
    state.battery = 100.0f;
    state.isGhost = false;
    state.isDead = false;
    state.isSpectator = false;
    state.flashlightOn = true;
    state.isRunning = false;
    state.headBob = 0.0f;
    state.footstepTimer = 0.0f;
    pitch = 0.0f;

    camera.position = state.position;
    camera.target = {0.0f, 0.0f, -1.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    updateCamera();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const PlayerState& Player::getState() const { return state; }
PlayerState& Player::getStateMut() { return state; }
uint32_t Player::getId() const { return state.id; }
Camera3D Player::getCamera() const { return camera; }

// ---------------------------------------------------------------------------
// Main update
// ---------------------------------------------------------------------------

void Player::update(float deltaTime, const uint8_t* mapData) {
    if (state.isDead || state.isSpectator) return;

    handleInput(deltaTime);

    // --- Stamina management ---
    if (state.isRunning && (state.stamina > STAMINA_MIN_TO_RUN)) {
        state.stamina -= STAMINA_DRAIN_RATE * deltaTime;
        if (state.stamina < 0.0f) state.stamina = 0.0f;
        if (state.stamina < STAMINA_MIN_TO_RUN) {
            state.isRunning = false;
        }
    } else {
        if (!state.isRunning) {
            state.stamina += STAMINA_REGEN_RATE * deltaTime;
            if (state.stamina > 100.0f) state.stamina = 100.0f;
        }
    }

    // --- Battery management ---
    if (state.flashlightOn) {
        state.battery -= BATTERY_DRAIN_RATE * deltaTime;
        if (state.battery < 0.0f) {
            state.battery = 0.0f;
            state.flashlightOn = false;
        }
    }

    // --- Sanity passive recovery (slow) ---
    if (state.sanity < 100.0f) {
        state.sanity += 1.0f * deltaTime;
        if (state.sanity > 100.0f) state.sanity = 100.0f;
    }

    // --- Head bob ---
    bool isMoving = (IsKeyDown(KEY_W) || IsKeyDown(KEY_S) ||
                     IsKeyDown(KEY_A) || IsKeyDown(KEY_D));
    if (isMoving && !state.isDead) {
        float bobSpeed = state.isRunning ? HEAD_BOB_RUN_SPEED : HEAD_BOB_WALK_SPEED;
        float targetAmp = state.isRunning ? HEAD_BOB_RUN_AMP : HEAD_BOB_WALK_AMP;
        headBobAmplitude += (targetAmp - headBobAmplitude) * 6.0f * deltaTime;
        headBobPhase += bobSpeed * deltaTime;
        state.headBob = sinf(headBobPhase) * headBobAmplitude;

        // Footstep timer
        float stepInterval = state.isRunning ? FOOTSTEP_RUN_INTERVAL : FOOTSTEP_WALK_INTERVAL;
        state.footstepTimer += deltaTime;
        if (state.footstepTimer >= stepInterval) {
            state.footstepTimer -= stepInterval;
            // Footstep sound event would be dispatched here by audio system
        }
    } else {
        headBobAmplitude *= (1.0f - 8.0f * deltaTime);
        if (headBobAmplitude < 0.001f) headBobAmplitude = 0.0f;
        headBobPhase += HEAD_BOB_WALK_SPEED * deltaTime;
        state.headBob = sinf(headBobPhase) * headBobAmplitude;
        state.footstepTimer = 0.0f;
    }

    updateCamera();
    (void)mapData; // mapData used via checkCollision from movement calls
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------

void Player::handleInput(float deltaTime) {
    if (state.isDead || state.isSpectator) return;

    // Rotation from mouse movement (yaw)
    Vector2 mouseDelta = GetMouseDelta();
    rotate(mouseDelta.x);

    // Pitch from mouse Y
    pitch -= mouseDelta.y * 0.003f;
    if (pitch > 1.5f)  pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;

    // Movement keys
    if (IsKeyDown(KEY_W)) moveForward(deltaTime);
    if (IsKeyDown(KEY_S)) moveBackward(deltaTime);
    if (IsKeyDown(KEY_A)) moveLeft(deltaTime);
    if (IsKeyDown(KEY_D)) moveRight(deltaTime);

    // Run toggle – hold shift
    toggleRun(IsKeyDown(KEY_LEFT_SHIFT));

    // Flashlight toggle
    if (IsKeyPressed(KEY_F)) toggleFlashlight();
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

void Player::moveForward(float deltaTime) {
    float speed = state.isRunning ? 5.5f : 3.0f;
    float yaw = state.rotation;
    Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 newPos = {
        state.position.x + forward.x * speed * deltaTime,
        state.position.y,
        state.position.z + forward.z * speed * deltaTime
    };
    if (!checkCollision(newPos, nullptr)) {
        state.position = newPos;
    }
}

void Player::moveBackward(float deltaTime) {
    float speed = state.isRunning ? 5.5f : 3.0f;
    float yaw = state.rotation;
    Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 newPos = {
        state.position.x - forward.x * speed * deltaTime,
        state.position.y,
        state.position.z - forward.z * speed * deltaTime
    };
    if (!checkCollision(newPos, nullptr)) {
        state.position = newPos;
    }
}

void Player::moveLeft(float deltaTime) {
    float speed = state.isRunning ? 5.5f : 3.0f;
    float yaw = state.rotation;
    // Right vector (perpendicular to forward in XZ plane)
    Vector3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    Vector3 newPos = {
        state.position.x - right.x * speed * deltaTime,
        state.position.y,
        state.position.z - right.z * speed * deltaTime
    };
    if (!checkCollision(newPos, nullptr)) {
        state.position = newPos;
    }
}

void Player::moveRight(float deltaTime) {
    float speed = state.isRunning ? 5.5f : 3.0f;
    float yaw = state.rotation;
    Vector3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    Vector3 newPos = {
        state.position.x + right.x * speed * deltaTime,
        state.position.y,
        state.position.z + right.z * speed * deltaTime
    };
    if (!checkCollision(newPos, nullptr)) {
        state.position = newPos;
    }
}

void Player::rotate(float deltaX) {
    float sensitivity = 0.003f;
    state.rotation += deltaX * sensitivity;
}

void Player::toggleRun(bool running) {
    if (running && state.stamina > STAMINA_MIN_TO_RUN) {
        state.isRunning = true;
    } else {
        state.isRunning = false;
    }
}

void Player::toggleFlashlight() {
    if (state.battery > 0.0f) {
        state.flashlightOn = !state.flashlightOn;
    }
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

void Player::updateCamera() {
    float eyeHeight = PLAYER_HEIGHT + state.headBob;

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
}

// ---------------------------------------------------------------------------
// Collision detection
// ---------------------------------------------------------------------------

bool Player::checkCollision(Vector3 newPos, const uint8_t* mapData) {
    // Test four corners of the player's bounding cylinder (approximated as box in XZ)
    float offsets[4][2] = {
        { PLAYER_RADIUS,  PLAYER_RADIUS},
        {-PLAYER_RADIUS,  PLAYER_RADIUS},
        { PLAYER_RADIUS, -PLAYER_RADIUS},
        {-PLAYER_RADIUS, -PLAYER_RADIUS}
    };

    // Access the global map grid shared with GameMap
    extern uint8_t g_mapGrid[50][50];

    for (int i = 0; i < 4; i++) {
        float testX = newPos.x + offsets[i][0];
        float testZ = newPos.z + offsets[i][1];

        int gx = static_cast<int>(testX / TILE_SIZE);
        int gz = static_cast<int>(testZ / TILE_SIZE);

        if (gx < 0 || gx >= MAP_WIDTH || gz < 0 || gz >= MAP_HEIGHT) {
            return true; // Out of bounds treated as wall
        }

        // Wall tiles block movement
        if (g_mapGrid[gz][gx] == 1) {
            return true;
        }
        // Closed door tiles also block (value 2 = closed door)
        if (g_mapGrid[gz][gx] == 2) {
            return true;
        }
    }

    (void)mapData; // Reserved for future per-call map data override
    return false;
}

// ---------------------------------------------------------------------------
// Damage / Death / Respawn
// ---------------------------------------------------------------------------

void Player::takeDamage(float amount) {
    if (state.isDead) return;
    state.sanity -= amount;
    if (state.sanity <= 0.0f) {
        state.sanity = 0.0f;
        die();
    }
}

void Player::die() {
    state.isDead = true;
    state.isSpectator = true;
    state.stamina = 0.0f;
}

void Player::respawn(Vector3 pos) {
    state.position = pos;
    state.isDead = false;
    state.isSpectator = false;
    state.stamina = 100.0f;
    state.sanity = 100.0f;
    state.battery = 100.0f;
    state.flashlightOn = true;
    state.isRunning = false;
    state.headBob = 0.0f;
    state.footstepTimer = 0.0f;
    headBobPhase = 0.0f;
    headBobAmplitude = 0.0f;
    pitch = 0.0f;
    updateCamera();
}
