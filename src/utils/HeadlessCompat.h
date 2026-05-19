#pragma once

// ============================================================================
// HeadlessCompat.h
// ============================================================================
// Provides raylib-compatible types and functions when KAIKAI_HEADLESS is
// defined, so the server can compile and run without raylib / OpenGL.
//
// When KAIKAI_HEADLESS is NOT defined, this header does nothing — the real
// raylib.h / raymath.h are included as usual.
//
// Vector3 is binary-compatible with raylib's Vector3 (same memory layout:
// three contiguous floats) so packet structs that embed Vector3 are
// interchangeable between headless-server and raylib-client builds.
// ============================================================================

#if defined(KAIKAI_HEADLESS)

#include <chrono>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Types — binary-compatible with raylib definitions
// ---------------------------------------------------------------------------

typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;

typedef struct Vector2 {
    float x;
    float y;
} Vector2;

// ---------------------------------------------------------------------------
// Timing — replaces raylib's GetTime() / GetFrameTime()
// ---------------------------------------------------------------------------

// Returns elapsed time in seconds since the first call (monotonic).
inline double GetTime()
{
    static const auto startTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - startTime).count();
}

// Approximate frame time for server tick rate.
inline float GetFrameTime()
{
    return 1.0f / 60.0f;
}

// ---------------------------------------------------------------------------
// raymath replacements — vector arithmetic
// ---------------------------------------------------------------------------

inline float Vector3Distance(Vector3 a, Vector3 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline Vector3 Vector3Subtract(Vector3 a, Vector3 b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vector3 Vector3Add(Vector3 a, Vector3 b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vector3 Vector3Scale(Vector3 v, float s)
{
    return { v.x * s, v.y * s, v.z * s };
}

inline float Vector3Length(Vector3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vector3 Vector3Normalize(Vector3 v)
{
    float len = Vector3Length(v);
    if (len < 1e-6f) return { 0.0f, 0.0f, 0.0f };
    float inv = 1.0f / len;
    return { v.x * inv, v.y * inv, v.z * inv };
}

// ---------------------------------------------------------------------------
// Clamp — replaces raylib's Clamp() template
// ---------------------------------------------------------------------------

template <typename T>
inline T Clamp(T value, T minVal, T maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

#endif // KAIKAI_HEADLESS
