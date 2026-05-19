#pragma once
#if defined(KAIKAI_HEADLESS)
#include "../utils/HeadlessCompat.h"
#else
#include "raylib.h"
#endif
#include <vector>
#include <cstdint>

struct PathNode {
    int x, z; // grid coordinates
    float gCost, hCost, fCost;
    PathNode* parent = nullptr;
    
    void calculateFCost() { fCost = gCost + hCost; }
};

class Pathfinding {
public:
    Pathfinding();
    ~Pathfinding() = default;
    
    // Main pathfinding method - A* algorithm
    std::vector<Vector3> findPath(Vector3 start, Vector3 end, const uint8_t* gridData);
    
    // Grid helpers
    static int worldToGridX(float x);
    static int worldToGridZ(float z);
    static Vector3 gridToWorld(int gx, int gz);
    static bool isValidCell(int gx, int gz, const uint8_t* gridData);
    
    // Heuristic
    static float calculateHCost(int x1, int z1, int x2, int z2);
    
    // Smooth path (remove unnecessary waypoints using wall data)
    static std::vector<Vector3> smoothPath(const std::vector<Vector3>& path, const uint8_t* gridData);
    
    // Line of sight check through grid cells (needed for smoothing and AI)
    static bool hasLineOfSight(Vector3 from, Vector3 to, const uint8_t* gridData);
    
private:
    static constexpr int DIRECTIONS = 8;
    static constexpr int dx[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static constexpr int dz[8] = {1, -1, 0, 0, 1, -1, -1, 1};
};
