#include "FogSystem.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------
FogSystem::FogSystem()
    : density(0.015f)
    , fogColor({20, 15, 25, 255})
    , animationTime(0.0f)
{
    generateFogPlanes();
}

// ----------------------------------------------------------------------------
// Generate semi-transparent fog planes scattered around the level
// ----------------------------------------------------------------------------
void FogSystem::generateFogPlanes()
{
    fogPlanes.clear();

    const int numPlanes = 60;
    const float spread  = 80.0f;  // world units spread

    for (int i = 0; i < numPlanes; ++i) {
        FogPlane plane;
        plane.position = {
            ((float)rand() / RAND_MAX - 0.5f) * spread,
            0.3f + ((float)rand() / RAND_MAX) * 1.5f,
            ((float)rand() / RAND_MAX - 0.5f) * spread
        };
        plane.width  = 4.0f + ((float)rand() / RAND_MAX) * 8.0f;
        plane.height = 2.0f + ((float)rand() / RAND_MAX) * 3.0f;
        plane.alpha  = 0.02f + ((float)rand() / RAND_MAX) * 0.06f;
        plane.speed  = 0.1f + ((float)rand() / RAND_MAX) * 0.4f;
        plane.phase  = ((float)rand() / RAND_MAX) * 2.0f * PI;
        fogPlanes.push_back(plane);
    }
}

// ----------------------------------------------------------------------------
// Per-frame update: animate fog planes and density oscillation
// ----------------------------------------------------------------------------
void FogSystem::update(float deltaTime)
{
    animationTime += deltaTime;

    // Slowly drift fog planes horizontally
    for (auto& plane : fogPlanes) {
        plane.position.x += sinf(animationTime * plane.speed + plane.phase) * deltaTime * 0.3f;
        plane.position.z += cosf(animationTime * plane.speed * 0.7f + plane.phase) * deltaTime * 0.2f;

        // Gentle vertical bob
        plane.position.y += sinf(animationTime * 0.5f + plane.phase) * deltaTime * 0.05f;
    }
}

// ----------------------------------------------------------------------------
// Render fog planes as semi-transparent billboards facing the camera
// This is the fallback / supplemental renderer when the shader pipeline
// is not active.  The primary fog is handled by the flashlight fragment
// shader (distance-based) and the post-process fog shader.
// ----------------------------------------------------------------------------
void FogSystem::render(const Camera3D& camera) const
{
    if (density < 0.001f) return;

    Vector3 camPos = camera.position;
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

    for (const auto& plane : fogPlanes) {
        // Only render fog planes within reasonable distance
        Vector3 toPlane = Vector3Subtract(plane.position, camPos);
        float dist = Vector3Length(toPlane);
        if (dist > 40.0f) continue;

        // Billboard: face the camera
        Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, {0.0f, 1.0f, 0.0f}));
        Vector3 up    = Vector3Normalize(Vector3CrossProduct(right, camForward));

        float halfW = plane.width  * 0.5f;
        float halfH = plane.height * 0.5f;

        Vector3 corners[4] = {
            Vector3Add(Vector3Add(plane.position, Vector3Scale(right, -halfW)), Vector3Scale(up, -halfH)),
            Vector3Add(Vector3Add(plane.position, Vector3Scale(right,  halfW)), Vector3Scale(up, -halfH)),
            Vector3Add(Vector3Add(plane.position, Vector3Scale(right,  halfW)), Vector3Scale(up,  halfH)),
            Vector3Add(Vector3Add(plane.position, Vector3Scale(right, -halfW)), Vector3Scale(up,  halfH))
        };

        // Distance-based alpha fade: closer planes are more visible
        float distFade = 1.0f - Clamp(dist / 40.0f, 0.0f, 1.0f);

        // Density multiplier affects alpha
        float densityAlpha = density / 0.015f; // normalized around default density

        // Animated pulse
        float pulse = 0.8f + 0.2f * sinf(animationTime * 0.5f + plane.phase);

        float finalAlpha = plane.alpha * distFade * densityAlpha * pulse;
        finalAlpha = Clamp(finalAlpha, 0.0f, 0.25f);

        unsigned char a = (unsigned char)(finalAlpha * 255.0f);
        Color planeColor = {fogColor.r, fogColor.g, fogColor.b, a};

        // Draw as a triangle strip (two triangles)
        DrawTriangle3D(corners[0], corners[1], corners[2], planeColor);
        DrawTriangle3D(corners[0], corners[2], corners[3], planeColor);
    }
}

// ----------------------------------------------------------------------------
// Mutators
// ----------------------------------------------------------------------------
void FogSystem::setFogDensity(float d)
{
    density = Clamp(d, 0.0f, 0.1f);
}

void FogSystem::setFogColor(Color color)
{
    fogColor = color;
}

// ----------------------------------------------------------------------------
// Accessors
// ----------------------------------------------------------------------------
float FogSystem::getFogDensity() const
{
    return density;
}
