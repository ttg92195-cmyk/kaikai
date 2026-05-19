#include "StaminaSystem.h"
#include "../utils/Constants.h"
#include <algorithm>
#include <cmath>

void StaminaSystem::update(float deltaTime, PlayerState& player) {
    // Dead, ghost, or spectator players don't use stamina
    if (player.isDead || player.isSpectator || player.isGhost) {
        // Still regen stamina in case they respawn
        player.stamina = std::min(player.stamina + STAMINA_REGEN_RATE * deltaTime, DEFAULT_STAMINA);
        return;
    }

    // Determine if the player is currently in an exhausted state
    bool currentlyExhausted = wasExhausted || player.stamina <= EXHAUSTED_THRESHOLD;

    if (currentlyExhausted) {
        // Exhausted: cannot run, regen stamina until recovery threshold is reached
        player.stamina += STAMINA_REGEN_RATE * deltaTime;

        // Only clear exhaustion once stamina recovers past the recovery threshold
        if (player.stamina >= RECOVERY_THRESHOLD) {
            wasExhausted = false;
        }
    } else {
        // Not exhausted — check if player is trying to run
        // We determine "running" by whether the player could run (has stamina, moving fast)
        // and their stamina is being consumed. The caller sets the running state via drain().
        // If not draining (not running), regenerate stamina.
        // Default behavior: regenerate. drain() is called externally when running.
        player.stamina += STAMINA_REGEN_RATE * deltaTime;

        // Check if stamina has dropped to exhaustion threshold (could happen from external drain)
        if (player.stamina <= EXHAUSTED_THRESHOLD) {
            wasExhausted = true;
        }
    }

    // Clamp stamina to valid range
    player.stamina = std::clamp(player.stamina, 0.0f, DEFAULT_STAMINA);
}

void StaminaSystem::drain(float amount) {
    // This method is designed to be called externally during the same frame
    // before or after update(). The caller should subtract the amount from
    // player.stamina directly. This method provides the interface for
    // external systems to request a stamina drain.
    // In practice, the caller does: player.stamina -= amount;
    // We keep this as an API hook for potential future effects (e.g. stamina potions,
    // poison drains, etc.) that might modify the effective drain amount.
}

void StaminaSystem::regenerate(float amount) {
    // Similar to drain(), this is an API hook for external systems.
    // The caller should add the amount to player.stamina directly.
}

bool StaminaSystem::canRun(const PlayerState& player) const {
    if (player.isDead || player.isSpectator || player.isGhost) {
        return false;
    }
    if (wasExhausted) {
        return false;
    }
    if (player.stamina <= EXHAUSTED_THRESHOLD) {
        return false;
    }
    return true;
}

float StaminaSystem::getStaminaPercentage(const PlayerState& player) const {
    if (DEFAULT_STAMINA <= 0.0f) return 0.0f;
    return player.stamina / DEFAULT_STAMINA;
}

bool StaminaSystem::isExhausted(const PlayerState& player) const {
    return wasExhausted || player.stamina <= EXHAUSTED_THRESHOLD;
}

float StaminaSystem::getMovementSpeedMultiplier(const PlayerState& player) const {
    if (player.isDead || player.isSpectator) {
        return 0.0f;
    }

    if (player.isGhost) {
        return 1.0f; // ghosts move at full speed
    }

    // When exhausted, movement speed is halved with smooth transition
    if (isExhausted(player)) {
        if (player.stamina <= EXHAUSTED_THRESHOLD) {
            return 0.5f;
        }
        // Smooth transition between EXHAUSTED_THRESHOLD and RECOVERY_THRESHOLD
        float t = (player.stamina - EXHAUSTED_THRESHOLD) / (RECOVERY_THRESHOLD - EXHAUSTED_THRESHOLD);
        t = std::clamp(t, 0.0f, 1.0f);
        // Smoothstep interpolation for nicer feel
        float smooth = t * t * (3.0f - 2.0f * t);
        return 0.5f + smooth * 0.5f;
    }

    // Normal state: slight slowdown when stamina is getting low but not yet exhausted
    if (player.stamina < RECOVERY_THRESHOLD) {
        float t = (player.stamina - EXHAUSTED_THRESHOLD) / (RECOVERY_THRESHOLD - EXHAUSTED_THRESHOLD);
        t = std::clamp(t, 0.0f, 1.0f);
        float smooth = t * t * (3.0f - 2.0f * t);
        return 0.5f + smooth * 0.5f;
    }

    return 1.0f;
}
