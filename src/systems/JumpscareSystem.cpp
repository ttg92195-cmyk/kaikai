#include "JumpscareSystem.h"
#include "../utils/Constants.h"
#include "../game/Player.h"
#include <algorithm>
#include <cmath>

using namespace Kaikai;

void JumpscareSystem::update(float deltaTime) {
    // Update cooldown timer
    if (cooldownTimer > 0.0f) {
        cooldownTimer -= deltaTime;
        if (cooldownTimer < 0.0f) {
            cooldownTimer = 0.0f;
        }
    }

    // Update all active jumpscare events
    for (auto& event : activeJumpscares) {
        if (!event.active) continue;

        event.timer -= deltaTime;

        // Deactivate when the jumpscare duration has elapsed
        if (event.timer <= 0.0f) {
            event.active = false;
            event.timer = 0.0f;
        }
    }

    // Decrease screen flash alpha over time
    if (screenFlashAlpha > 0.0f) {
        // Flash fades out over ~0.3 seconds
        screenFlashAlpha -= deltaTime * 3.3f;
        if (screenFlashAlpha < 0.0f) {
            screenFlashAlpha = 0.0f;
        }
    }

    // Clean up inactive events that have fully expired
    activeJumpscares.erase(
        std::remove_if(activeJumpscares.begin(), activeJumpscares.end(),
            [](const JumpscareEvent& e) { return !e.active; }),
        activeJumpscares.end()
    );
}

void JumpscareSystem::triggerJumpscare(uint32_t playerId, Vector3 ghostPos, float intensity) {
    JumpscareEvent event;
    event.targetPlayerId = playerId;
    event.duration = JUMPSCARE_DURATION;
    event.timer = JUMPSCARE_DURATION;
    event.active = true;
    event.ghostPosition = ghostPos;
    event.intensity = std::clamp(intensity, 0.0f, 2.0f);

    activeJumpscares.push_back(event);

    // Set screen flash to full white
    screenFlashAlpha = 1.0f;

    // Reset cooldown
    cooldownTimer = MIN_COOLDOWN;

    // Reset alone timer to prevent immediate re-trigger
    aloneTimer = 0.0f;
}

#if !defined(KAIKAI_HEADLESS)
void JumpscareSystem::render(const Camera3D& camera) const {
    for (const auto& event : activeJumpscares) {
        if (!event.active) continue;

        float progress = 1.0f - (event.timer / event.duration); // 0.0 at start, 1.0 at end
        float ghostIntensity = event.intensity;

        // === Ghost Model Rendering ===
        // Position the ghost 0.5 meters in front of the camera
        Vector3 cameraForward = {
            cosf(camera.fovy * 0.01745329f) * sinf(camera.target.x - camera.position.x),
            0.0f,
            cosf(camera.fovy * 0.01745329f) * cosf(camera.target.z - camera.position.z)
        };

        // Calculate actual forward direction from camera
        Vector3 forward = Vector3Subtract(camera.target, camera.position);
        forward = Vector3Normalize(forward);

        Vector3 ghostPos = Vector3Add(camera.position, Vector3Scale(forward, 0.5f));
        ghostPos.y += 0.2f; // Slightly above eye level for maximum scare

        // === Animation: Growing effect ===
        // Ghost starts small and grows rapidly
        float scaleProgress = progress < 0.2f ? progress / 0.2f : 1.0f;
        float baseScale = 0.3f + scaleProgress * 0.7f; // Grows from 0.3 to 1.0
        baseScale *= ghostIntensity;

        // === Animation: Shaking effect ===
        float shakeX = sinf(progress * 50.0f) * 0.03f * ghostIntensity;
        float shakeY = cosf(progress * 40.0f) * 0.02f * ghostIntensity;
        ghostPos.x += shakeX;
        ghostPos.y += shakeY;

        // === Animation: Distortion effect ===
        // Slight pulsing scale
        float pulseScale = 1.0f + sinf(progress * 30.0f) * 0.1f * ghostIntensity;
        float finalScale = baseScale * pulseScale;

        // Draw ghost body as a tall, dark figure using raylib primitives
        // Main body - dark torso
        Color ghostColor = { 20, 15, 25, 220 };
        Color ghostDetailColor = { 40, 30, 50, 200 };

        // Body: tall thin cylinder
        DrawCylinder(ghostPos, 0.15f * finalScale, 0.1f * finalScale, 1.2f * finalScale, 8, ghostColor);

        // Head: sphere on top
        Vector3 headPos = ghostPos;
        headPos.y += 0.7f * finalScale;
        DrawSphere(headPos, 0.15f * finalScale, ghostColor);

        // Eyes: glowing red dots
        Vector3 leftEyePos = headPos;
        leftEyePos.x -= 0.05f * finalScale;
        leftEyePos.z += 0.12f * finalScale;
        Vector3 rightEyePos = headPos;
        rightEyePos.x += 0.05f * finalScale;
        rightEyePos.z += 0.12f * finalScale;

        float eyeGlow = 0.8f + sinf(progress * 20.0f) * 0.2f;
        Color eyeColor = {
            (unsigned char)(200 * eyeGlow * ghostIntensity),
            (unsigned char)(20 * eyeGlow),
            (unsigned char)(20 * eyeGlow),
            255
        };
        DrawSphere(leftEyePos, 0.025f * finalScale, eyeColor);
        DrawSphere(rightEyePos, 0.025f * finalScale, eyeColor);

        // Arms: two thin cylinders reaching toward camera
        Vector3 leftArmBase = ghostPos;
        leftArmBase.x -= 0.15f * finalScale;
        leftArmBase.y += 0.3f * finalScale;
        Vector3 rightArmBase = ghostPos;
        rightArmBase.x += 0.15f * finalScale;
        rightArmBase.y += 0.3f * finalScale;

        // Arms reaching forward
        Vector3 armEnd = Vector3Add(ghostPos, Vector3Scale(forward, 0.3f));
        armEnd.y -= 0.1f * finalScale;

        // Left arm
        Vector3 leftArmEnd = armEnd;
        leftArmEnd.x -= 0.1f * finalScale;
        DrawCylinderEx(leftArmBase, leftArmEnd, 0.03f * finalScale, 0.02f * finalScale, 6, ghostDetailColor);

        // Right arm
        Vector3 rightArmEnd = armEnd;
        rightArmEnd.x += 0.1f * finalScale;
        DrawCylinderEx(rightArmBase, rightArmEnd, 0.03f * finalScale, 0.02f * finalScale, 6, ghostDetailColor);

        // === Dark aura particles around ghost ===
        for (int i = 0; i < 8; i++) {
            float angle = progress * 5.0f + i * 0.785f; // distributed around ghost
            float radius = 0.3f * finalScale;
            Vector3 particlePos = ghostPos;
            particlePos.x += cosf(angle) * radius;
            particlePos.z += sinf(angle) * radius;
            particlePos.y += sinf(progress * 10.0f + i) * 0.1f;

            float particleAlpha = (1.0f - progress) * 0.5f;
            Color particleColor = { 80, 40, 100, (unsigned char)(particleAlpha * 255) };
            DrawSphere(particlePos, 0.02f, particleColor);
        }
    }

    // Render screen flash effect
    renderFlashEffect();

    // Render heavy screen shake during active jumpscare
    for (const auto& event : activeJumpscares) {
        if (!event.active) continue;

        float progress = 1.0f - (event.timer / event.duration);
        // Heavy red tint overlay that fades
        float redAlpha = (1.0f - progress) * 0.4f * event.intensity;
        Color redOverlay = { 150, 0, 0, (unsigned char)(redAlpha * 255) };
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), redOverlay);

        // Screen distortion bars
        int numBars = 5;
        for (int i = 0; i < numBars; i++) {
            float yOffset = sinf(progress * 30.0f + i * 2.0f) * GetScreenHeight() * 0.5f;
            int barY = (int)(GetScreenHeight() * 0.5f + yOffset + i * 20 - 50);
            int barHeight = 2 + (int)(sinf(progress * 20.0f + i) * 3.0f);
            barHeight = std::abs(barHeight);
            Color glitchColor = { 0, 0, 0, (unsigned char)((1.0f - progress) * 100) };
            DrawRectangle(0, barY, GetScreenWidth(), barHeight, glitchColor);
        }
    }
}
#endif // !KAIKAI_HEADLESS

#if !defined(KAIKAI_HEADLESS)
void JumpscareSystem::renderFlashEffect() const {
    if (screenFlashAlpha <= 0.0f) return;

    // White flash overlay that fades out
    Color flashColor = { 255, 255, 255, (unsigned char)(screenFlashAlpha * 255) };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), flashColor);
}
#endif // !KAIKAI_HEADLESS

bool JumpscareSystem::isActive() const {
    for (const auto& event : activeJumpscares) {
        if (event.active) return true;
    }
    return false;
}

bool JumpscareSystem::checkDynamicTrigger(const PlayerState& player, float deltaTime,
                                           bool aloneInRoom, float sanity) {
    // Cannot trigger if player is dead or already in a jumpscare
    if (player.isDead || player.isSpectator) {
        return false;
    }

    // Cannot trigger if cooldown hasn't expired
    if (cooldownTimer > 0.0f) {
        return false;
    }

    // Track how long the player has been alone in a room
    if (aloneInRoom) {
        aloneTimer += deltaTime;
    } else {
        aloneTimer = 0.0f;
        return false;
    }

    // Check trigger conditions:
    // 1. Player has been alone for ALONE_TRIGGER_TIME seconds
    // 2. Player's sanity is below 40%
    // 3. Cooldown has expired (already checked above)
    if (aloneTimer >= ALONE_TRIGGER_TIME && sanity < 40.0f) {
        // Trigger the jumpscare
        triggerJumpscare(player.id, player.position, 1.0f);
        return true;
    }

    return false;
}
