#include "SanitySystem.h"
#include "../utils/Constants.h"
#include <algorithm>
#include <cmath>

void SanitySystem::update(float deltaTime, PlayerState& player, bool inDarkArea, float ghostDistance) {
    // Dead or spectator players don't have sanity mechanics
    if (player.isDead || player.isSpectator) {
        return;
    }

    // === Sanity Drain ===
    float drainAmount = 0.0f;

    // Drain sanity when in a dark area (no flashlight or no ambient light)
    if (inDarkArea) {
        drainAmount += SANITY_DRAIN_RATE * deltaTime;
    }

    // Additional drain when ghost is nearby
    // The closer the ghost, the stronger the drain
    if (ghostDistance < GHOST_SANITY_DRAIN_RANGE && ghostDistance >= 0.0f) {
        // Proportional drain: stronger when ghost is closer
        float proximityFactor = 1.0f - (ghostDistance / GHOST_SANITY_DRAIN_RANGE);
        proximityFactor = proximityFactor * proximityFactor; // quadratic falloff for intensity
        // Up to 3x the base drain rate when ghost is very close
        float ghostDrain = SANITY_DRAIN_RATE * 3.0f * proximityFactor * deltaTime;
        drainAmount += ghostDrain;
    }

    // === Sanity Regeneration ===
    float regenAmount = 0.0f;

    // Regen sanity when in light (flashlight on) and no ghost nearby
    if (!inDarkArea && player.flashlightOn && ghostDistance >= GHOST_SANITY_DRAIN_RANGE) {
        regenAmount += SANITY_REGEN_RATE * deltaTime;
    }

    // Apply net sanity change
    player.sanity -= drainAmount;
    player.sanity += regenAmount;

    // Clamp sanity to valid range
    player.sanity = std::clamp(player.sanity, 0.0f, DEFAULT_SANITY);

    // === Hallucination Timer ===
    if (player.sanity < HALLUCINATION_THRESHOLD) {
        hallucinationTimer += deltaTime;

        // Lower sanity = more frequent hallucinations
        // At sanity = 0, hallucinations every 5 seconds
        // At sanity = HALLUCINATION_THRESHOLD, hallucinations every ~30 seconds
        float sanityFraction = player.sanity / HALLUCINATION_THRESHOLD;
        nextHallucinationTime = 5.0f + sanityFraction * 25.0f;
    } else {
        // Reset hallucination timer when sanity is healthy
        hallucinationTimer = 0.0f;
        nextHallucinationTime = 30.0f;
    }
}

float SanitySystem::getSanityPercentage(const PlayerState& player) const {
    if (DEFAULT_SANITY <= 0.0f) return 0.0f;
    return player.sanity / DEFAULT_SANITY;
}

float SanitySystem::getScreenShakeIntensity(const PlayerState& player) const {
    // No shake above 60 sanity
    if (player.sanity >= 60.0f) {
        return 0.0f;
    }

    // Shake increases from 0 to 1.0 as sanity goes from 60 to 0
    float t = 1.0f - (player.sanity / 60.0f);
    // Use quadratic curve for more dramatic effect at low sanity
    return t * t;
}

float SanitySystem::getVignetteIntensity(const PlayerState& player) const {
    // Vignette (dark edges) intensifies as sanity drops
    // At full sanity, no vignette
    // At 0 sanity, maximum vignette
    if (player.sanity >= DEFAULT_SANITY) {
        return 0.0f;
    }

    float t = 1.0f - (player.sanity / DEFAULT_SANITY);
    // Vignette starts becoming noticeable around 70% sanity
    if (t < 0.3f) {
        return 0.0f;
    }
    float adjusted = (t - 0.3f) / 0.7f;
    return std::clamp(adjusted, 0.0f, 1.0f);
}

float SanitySystem::getColorDistortion(const PlayerState& player) const {
    // Red tint increases as sanity drops
    // Starts becoming noticeable below 50 sanity
    if (player.sanity >= 50.0f) {
        return 0.0f;
    }

    float t = 1.0f - (player.sanity / 50.0f);
    // Smooth ramp with exponential feel
    return std::clamp(t * t, 0.0f, 1.0f);
}

float SanitySystem::getAudioDistortion(const PlayerState& player) const {
    // Audio distortion (static/noise overlay) increases as sanity drops
    // Starts below 60 sanity
    if (player.sanity >= 60.0f) {
        return 0.0f;
    }

    float t = 1.0f - (player.sanity / 60.0f);
    return std::clamp(t, 0.0f, 1.0f);
}

bool SanitySystem::shouldTriggerJumpscare(const PlayerState& player) const {
    // Jumpscare can trigger when sanity is critically low
    return player.sanity < JUMPSCARE_SANITY_THRESHOLD && !player.isDead;
}

bool SanitySystem::isHallucinating(const PlayerState& player) const {
    // Currently hallucinating if timer has exceeded the next hallucination time
    // and sanity is below the threshold
    if (player.sanity >= HALLUCINATION_THRESHOLD) {
        return false;
    }

    return hallucinationTimer >= nextHallucinationTime;
}
