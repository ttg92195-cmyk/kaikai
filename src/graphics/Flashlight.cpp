#include "Flashlight.h"
#include "raymath.h"
#include <cmath>

using namespace Kaikai;

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------
Flashlight::Flashlight()
    : on(true)
    , battery(DEFAULT_BATTERY)
    , flickerTimer(0.0f)
    , flickerIntensity(1.0f)
    , position({0.0f, 0.0f, 0.0f})
    , direction({0.0f, 0.0f, -1.0f})
{
}

// ----------------------------------------------------------------------------
// Per-frame update
// ----------------------------------------------------------------------------
void Flashlight::update(float deltaTime, Vector3 playerPos, Vector3 playerForward, float batteryLevel)
{
    // Sync position & direction from the player camera
    position = playerPos;
    direction = playerForward;

    // Use the battery level supplied by the game state
    battery = batteryLevel;

    // Drain battery when flashlight is on (game logic handles actual drain,
    // but we mirror it here for the standalone case / intensity calculation)
    if (on && battery > 0.0f) {
        battery -= BATTERY_DRAIN_RATE * deltaTime;
        if (battery < 0.0f) battery = 0.0f;
    }

    // Auto-off when battery is depleted
    if (battery <= 0.0f) {
        on = false;
        battery = 0.0f;
    }

    // Update flicker effect when battery is low
    updateFlicker(deltaTime, battery);
}

// ----------------------------------------------------------------------------
// Flicker logic – rapid on/off with pseudo-random noise pattern
// ----------------------------------------------------------------------------
void Flashlight::updateFlicker(float deltaTime, float batteryLevel)
{
    flickerTimer += deltaTime;

    if (!on || batteryLevel > FLASHLIGHT_FLICKER_THRESHOLD) {
        // No flicker when battery is healthy or light is off
        flickerIntensity = on ? 1.0f : 0.0f;
        return;
    }

    // Below the flicker threshold, start erratic behavior
    float severity = 1.0f - (batteryLevel / FLASHLIGHT_FLICKER_THRESHOLD); // 0..1
    severity = Clamp(severity, 0.0f, 1.0f);

    // Multi-frequency noise for organic flicker
    float n1 = sinf(flickerTimer * 23.7f) * 0.5f + 0.5f;
    float n2 = sinf(flickerTimer * 47.3f + 1.7f) * 0.5f + 0.5f;
    float n3 = sinf(flickerTimer * 97.1f + 3.2f) * 0.5f + 0.5f;

    float combined = n1 * 0.5f + n2 * 0.3f + n3 * 0.2f;

    // The lower the battery, the more likely a dropout
    float dropoutThreshold = 0.3f + severity * 0.5f; // rises from 0.3 to 0.8

    if (combined < dropoutThreshold) {
        // Full dropout
        flickerIntensity = 0.0f;
    } else if (combined < dropoutThreshold + 0.15f) {
        // Dim flicker
        flickerIntensity = 0.3f + (combined - dropoutThreshold) * 4.67f; // 0.3..1.0
    } else {
        // Full brightness
        flickerIntensity = 1.0f;
    }

    // Very low battery: longer dropouts
    if (batteryLevel < 5.0f && batteryLevel > 0.0f) {
        float lowBatteryPhase = sinf(flickerTimer * 5.0f) * 0.5f + 0.5f;
        if (lowBatteryPhase < 0.7f) {
            flickerIntensity = 0.0f;
        }
    }

    // Force off if flashlight is off
    if (!on) {
        flickerIntensity = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// Render the visible flashlight beam cone
// ----------------------------------------------------------------------------
void Flashlight::render() const
{
    if (!on || flickerIntensity <= 0.0f) return;

    Vector3 beamOrigin = position;
    // Offset slightly forward so the beam starts in front of the player
    beamOrigin.x += direction.x * 0.3f;
    beamOrigin.z += direction.z * 0.3f;

    const float beamLength = 12.0f;
    const float beamRadius  = 3.0f;

    // Calculate the endpoint of the beam
    Vector3 beamEnd = {
        beamOrigin.x + direction.x * beamLength,
        beamOrigin.y + direction.y * beamLength,
        beamOrigin.z + direction.z * beamLength
    };

    // Perpendicular vectors for the cone base
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 right = Vector3CrossProduct(direction, up);
    if (Vector3Length(right) < 0.001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    right = Vector3Normalize(right);
    up = Vector3Normalize(Vector3CrossProduct(right, direction));

    // Draw the cone as wireframe rings connected by lines
    const int segments = 16;
    const float alpha = 0.12f * flickerIntensity;
    Color beamColor = {
        (unsigned char)(255 * flickerIntensity),
        (unsigned char)(245 * flickerIntensity),
        (unsigned char)(200 * flickerIntensity),
        (unsigned char)(255 * alpha)
    };

    // Draw rings at various distances
    for (int ring = 1; ring <= 4; ++ring) {
        float t = (float)ring / 4.0f;
        float ringDist = beamLength * t;
        float ringRad  = beamRadius * t;

        Vector3 ringCenter = {
            beamOrigin.x + direction.x * ringDist,
            beamOrigin.y + direction.y * ringDist,
            beamOrigin.z + direction.z * ringDist
        };

        // Draw ring circle
        for (int i = 0; i < segments; ++i) {
            float angle1 = (float)i / segments * 2.0f * PI;
            float angle2 = (float)((i + 1) % segments) / segments * 2.0f * PI;

            Vector3 p1 = {
                ringCenter.x + (right.x * cosf(angle1) + up.x * sinf(angle1)) * ringRad,
                ringCenter.y + (right.y * cosf(angle1) + up.y * sinf(angle1)) * ringRad,
                ringCenter.z + (right.z * cosf(angle1) + up.z * sinf(angle1)) * ringRad
            };
            Vector3 p2 = {
                ringCenter.x + (right.x * cosf(angle2) + up.x * sinf(angle2)) * ringRad,
                ringCenter.y + (right.y * cosf(angle2) + up.y * sinf(angle2)) * ringRad,
                ringCenter.z + (right.z * cosf(angle2) + up.z * sinf(angle2)) * ringRad
            };

            DrawLine3D(p1, p2, beamColor);
        }
    }

    // Draw lines from origin to the outermost ring
    Vector3 lastRingCenter = {
        beamOrigin.x + direction.x * beamLength,
        beamOrigin.y + direction.y * beamLength,
        beamOrigin.z + direction.z * beamLength
    };

    for (int i = 0; i < segments; i += 2) {
        float angle = (float)i / segments * 2.0f * PI;
        Vector3 p = {
            lastRingCenter.x + (right.x * cosf(angle) + up.x * sinf(angle)) * beamRadius,
            lastRingCenter.y + (right.y * cosf(angle) + up.y * sinf(angle)) * beamRadius,
            lastRingCenter.z + (right.z * cosf(angle) + up.z * sinf(angle)) * beamRadius
        };
        DrawLine3D(beamOrigin, p, beamColor);
    }

    // Draw a small bright sphere at the flashlight source
    float glowAlpha = 0.3f * flickerIntensity;
    Color glowColor = {
        (unsigned char)(255 * flickerIntensity),
        (unsigned char)(250 * flickerIntensity),
        (unsigned char)(220 * flickerIntensity),
        (unsigned char)(255 * glowAlpha)
    };
    DrawSphere(beamOrigin, 0.08f, glowColor);
}

// ----------------------------------------------------------------------------
// Toggle flashlight on/off
// ----------------------------------------------------------------------------
void Flashlight::toggle()
{
    if (battery > 0.0f) {
        on = !on;
    }
    if (!on) {
        flickerIntensity = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// Accessors
// ----------------------------------------------------------------------------
bool Flashlight::isOn() const
{
    return on && battery > 0.0f;
}

float Flashlight::getBatteryLevel() const
{
    return battery;
}

void Flashlight::recharge(float amount)
{
    battery += amount;
    if (battery > DEFAULT_BATTERY) battery = DEFAULT_BATTERY;
    if (battery > 0.0f && !on) {
        // Don't auto-turn on, but allow it to be toggled on again
    }
}

float Flashlight::getIntensity() const
{
    if (!on) return 0.0f;
    // Intensity scales with battery when battery is moderate, and flicker when low
    if (battery > FLASHLIGHT_FLICKER_THRESHOLD) {
        return 1.0f;
    }
    return flickerIntensity;
}

Vector3 Flashlight::getPosition() const
{
    return position;
}

Vector3 Flashlight::getDirection() const
{
    return direction;
}
