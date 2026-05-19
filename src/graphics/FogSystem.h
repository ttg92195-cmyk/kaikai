#pragma once
#include "raylib.h"
#include <vector>

class FogSystem {
public:
    FogSystem();
    void update(float deltaTime);
    void render(const Camera3D& camera) const;

    void setFogDensity(float density);
    void setFogColor(Color color);
    float getFogDensity() const;

private:
    float density = 0.015f;
    Color fogColor = {20, 15, 25, 255}; // dark purple-ish
    float animationTime = 0.0f;

    // Fog particle planes for fallback rendering when shaders aren't available
    struct FogPlane {
        Vector3 position;
        float width;
        float height;
        float alpha;
        float speed;
        float phase;
    };
    std::vector<FogPlane> fogPlanes;

    void generateFogPlanes();
};
