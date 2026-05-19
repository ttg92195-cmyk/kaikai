#pragma once

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <utility>

// raylib Vector3 is available when raylib is included; otherwise provide a
// minimal stand-alone definition so this header compiles in server builds.
#ifndef RAYLIB_H
typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;
#endif

#include "Constants.h"

namespace Kaikai::Math {

// ---------------------------------------------------------------------------
// Scalar helpers
// ---------------------------------------------------------------------------

inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline float clamp(float value, float minVal, float maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

// ---------------------------------------------------------------------------
// Vector helpers
// ---------------------------------------------------------------------------

inline float distance(Vector3 a, Vector3 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float distanceXZ(Vector3 a, Vector3 b)
{
    float dx = b.x - a.x;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dz * dz);
}

inline Vector3 normalize(Vector3 v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f) return {0.0f, 0.0f, 0.0f};
    float inv = 1.0f / len;
    return {v.x * inv, v.y * inv, v.z * inv};
}

inline float dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 cross(Vector3 a, Vector3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Vector3 subtract(Vector3 a, Vector3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vector3 add(Vector3 a, Vector3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3 scale(Vector3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

inline float length(Vector3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline float lengthXZ(Vector3 v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}

// ---------------------------------------------------------------------------
// Angle helpers
// ---------------------------------------------------------------------------

inline float degToRad(float deg) { return deg * (3.14159265358979323846f / 180.0f); }
inline float radToDeg(float rad) { return rad * (180.0f / 3.14159265358979323846f); }

// Returns a direction vector from a yaw angle (rotation around Y axis).
// yaw = 0 → looking along +Z, yaw = 90 → looking along +X.
inline Vector3 directionFromYaw(float yaw)
{
    float rad = degToRad(yaw);
    return {std::sin(rad), 0.0f, std::cos(rad)};
}

// Returns the yaw angle (degrees) from a direction vector (ignores Y).
inline float yawFromDirection(Vector3 dir)
{
    return radToDeg(std::atan2(dir.x, dir.z));
}

// ---------------------------------------------------------------------------
// Random helpers
// ---------------------------------------------------------------------------

inline float randomFloat(float minVal, float maxVal)
{
    float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minVal + r * (maxVal - minVal);
}

inline int randomInt(int minVal, int maxVal)
{
    if (minVal >= maxVal) return minVal;
    return minVal + (std::rand() % (maxVal - minVal + 1));
}

// Seeded variant – useful for deterministic server-side generation.
inline float randomFloatSeeded(unsigned int& seed, float minVal, float maxVal)
{
    // Simple LCG for reproducibility
    seed = seed * 1664525u + 1013904223u;
    float r = static_cast<float>(seed >> 8) / static_cast<float>(0x00FFFFFF);
    return minVal + r * (maxVal - minVal);
}

inline int randomIntSeeded(unsigned int& seed, int minVal, int maxVal)
{
    seed = seed * 1664525u + 1013904223u;
    if (minVal >= maxVal) return minVal;
    return minVal + static_cast<int>((seed >> 8) % static_cast<unsigned int>(maxVal - minVal + 1));
}

// ---------------------------------------------------------------------------
// World ↔ Grid coordinate conversion
// ---------------------------------------------------------------------------

// Convert a world-space position to grid coordinates (tile indices).
// Returns {-1, -1} if the position is outside the map bounds.
inline std::pair<int, int> worldToGrid(Vector3 pos)
{
    int gx = static_cast<int>(std::floor(pos.x / TILE_SIZE));
    int gy = static_cast<int>(std::floor(pos.z / TILE_SIZE));  // Z maps to grid Y
    if (gx < 0 || gx >= MAP_WIDTH || gy < 0 || gy >= MAP_HEIGHT) {
        return {-1, -1};
    }
    return {gx, gy};
}

// Convert grid coordinates to a world-space position (center of the tile).
inline Vector3 gridToWorld(int gx, int gy)
{
    return {
        (static_cast<float>(gx) + 0.5f) * TILE_SIZE,
        0.0f,
        (static_cast<float>(gy) + 0.5f) * TILE_SIZE
    };
}

// Check whether a grid cell is valid (within bounds).
inline bool isValidGridCell(int gx, int gy)
{
    return gx >= 0 && gx < MAP_WIDTH && gy >= 0 && gy < MAP_HEIGHT;
}

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------

// Smooth-step interpolation (Hermite).
inline float smoothstep(float edge0, float edge1, float x)
{
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Check if a world position is inside the map boundaries.
inline bool isInsideMap(Vector3 pos)
{
    return pos.x >= 0.0f && pos.x < MAP_WORLD_WIDTH &&
           pos.z >= 0.0f && pos.z < MAP_WORLD_HEIGHT;
}

}  // namespace Kaikai::Math
