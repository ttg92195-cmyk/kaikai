#include "Pathfinding.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

// Map constants matching the game's global definitions
static constexpr int MAP_W = 50;
static constexpr int MAP_H = 50;
static constexpr float TILE_SZ = 2.0f;

Pathfinding::Pathfinding() = default;

int Pathfinding::worldToGridX(float x) {
    return static_cast<int>(std::floor(x / TILE_SZ));
}

int Pathfinding::worldToGridZ(float z) {
    return static_cast<int>(std::floor(z / TILE_SZ));
}

Vector3 Pathfinding::gridToWorld(int gx, int gz) {
    return {
        (static_cast<float>(gx) + 0.5f) * TILE_SZ,
        0.0f,
        (static_cast<float>(gz) + 0.5f) * TILE_SZ
    };
}

bool Pathfinding::isValidCell(int gx, int gz, const uint8_t* gridData) {
    if (gx < 0 || gx >= MAP_W || gz < 0 || gz >= MAP_H) return false;
    if (!gridData) return false;
    // A non-zero cell value indicates a wall
    return gridData[gz * MAP_W + gx] == 0;
}

float Pathfinding::calculateHCost(int x1, int z1, int x2, int z2) {
    // Octile distance heuristic for 8-directional movement
    int adx = std::abs(x2 - x1);
    int adz = std::abs(z2 - z1);
    float diagonalSteps = static_cast<float>(std::min(adx, adz));
    float straightSteps = static_cast<float>(std::abs(adx - adz));
    return diagonalSteps * 1.414f + straightSteps * 1.0f;
}

std::vector<Vector3> Pathfinding::findPath(Vector3 start, Vector3 end, const uint8_t* gridData) {
    std::vector<Vector3> result;

    if (!gridData) return result;

    int startX = worldToGridX(start.x);
    int startZ = worldToGridZ(start.z);
    int endX   = worldToGridX(end.x);
    int endZ   = worldToGridZ(end.z);

    // Clamp to map bounds
    startX = std::clamp(startX, 0, MAP_W - 1);
    startZ = std::clamp(startZ, 0, MAP_H - 1);
    endX   = std::clamp(endX, 0, MAP_W - 1);
    endZ   = std::clamp(endZ, 0, MAP_H - 1);

    // If start is inside a wall, search outward for the nearest valid cell
    if (!isValidCell(startX, startZ, gridData)) {
        bool found = false;
        for (int r = 1; r <= 5 && !found; ++r) {
            for (int dz = -r; dz <= r && !found; ++dz) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (std::abs(dx) != r && std::abs(dz) != r) continue; // only perimeter
                    if (isValidCell(startX + dx, startZ + dz, gridData)) {
                        startX += dx;
                        startZ += dz;
                        found = true;
                    }
                }
            }
        }
        if (!found) return result;
    }

    // If end is inside a wall, search outward for the nearest valid cell
    if (!isValidCell(endX, endZ, gridData)) {
        bool found = false;
        for (int r = 1; r <= 5 && !found; ++r) {
            for (int dz = -r; dz <= r && !found; ++dz) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (std::abs(dx) != r && std::abs(dz) != r) continue;
                    if (isValidCell(endX + dx, endZ + dz, gridData)) {
                        endX += dx;
                        endZ += dz;
                        found = true;
                    }
                }
            }
        }
        if (!found) return result;
    }

    // Already at destination
    if (startX == endX && startZ == endZ) {
        result.push_back(gridToWorld(endX, endZ));
        return result;
    }

    // Allocate node pool indexed by grid position
    const int totalCells = MAP_W * MAP_H;
    std::vector<PathNode> nodes(totalCells);
    for (int z = 0; z < MAP_H; ++z) {
        for (int x = 0; x < MAP_W; ++x) {
            int idx = z * MAP_W + x;
            nodes[idx].x = x;
            nodes[idx].z = z;
            nodes[idx].gCost = FLT_MAX;
            nodes[idx].hCost = 0.0f;
            nodes[idx].fCost = FLT_MAX;
            nodes[idx].parent = nullptr;
        }
    }

    // Closed list: tracks nodes already evaluated
    std::vector<bool> closed(totalCells, false);

    // Open list: simple vector of node indices
    std::vector<int> open;
    open.reserve(512);

    int startIdx = startZ * MAP_W + startX;
    nodes[startIdx].gCost = 0.0f;
    nodes[startIdx].hCost = calculateHCost(startX, startZ, endX, endZ);
    nodes[startIdx].calculateFCost();
    open.push_back(startIdx);

    while (!open.empty()) {
        // Find the node with the lowest fCost (break ties with lower hCost)
        int bestSlot = 0;
        float bestF = nodes[open[0]].fCost;
        float bestH = nodes[open[0]].hCost;
        for (size_t i = 1; i < open.size(); ++i) {
            float f = nodes[open[i]].fCost;
            float h = nodes[open[i]].hCost;
            if (f < bestF || (f == bestF && h < bestH)) {
                bestF = f;
                bestH = h;
                bestSlot = static_cast<int>(i);
            }
        }

        int currentIdx = open[bestSlot];
        PathNode& current = nodes[currentIdx];

        // Remove current from the open list (swap with last and pop)
        open[bestSlot] = open.back();
        open.pop_back();

        // Mark as closed
        closed[currentIdx] = true;

        // Goal reached — reconstruct the path
        if (current.x == endX && current.z == endZ) {
            std::vector<Vector3> reversePath;
            PathNode* node = &current;
            while (node != nullptr) {
                reversePath.push_back(gridToWorld(node->x, node->z));
                node = node->parent;
            }
            result.assign(reversePath.rbegin(), reversePath.rend());
            return smoothPath(result, gridData);
        }

        // Explore all 8 neighbours
        for (int d = 0; d < DIRECTIONS; ++d) {
            int nx = current.x + dx[d];
            int nz = current.z + dz[d];

            // Boundary check
            if (nx < 0 || nx >= MAP_W || nz < 0 || nz >= MAP_H) continue;

            int nIdx = nz * MAP_W + nx;

            // Skip walls and already-evaluated nodes
            if (!isValidCell(nx, nz, gridData)) continue;
            if (closed[nIdx]) continue;

            // For diagonal moves, ensure we're not corner-cutting through walls
            bool diagonal = (dx[d] != 0 && dz[d] != 0);
            if (diagonal) {
                // Both adjacent cardinal cells must be walkable
                if (!isValidCell(current.x + dx[d], current.z, gridData) ||
                    !isValidCell(current.x, current.z + dz[d], gridData)) {
                    continue;
                }
            }

            float moveCost = diagonal ? 1.414f : 1.0f;
            float tentativeG = current.gCost + moveCost;

            PathNode& neighbour = nodes[nIdx];

            if (tentativeG < neighbour.gCost) {
                neighbour.gCost = tentativeG;
                neighbour.hCost = calculateHCost(nx, nz, endX, endZ);
                neighbour.calculateFCost();
                neighbour.parent = &current;

                // Add to open list if not already present
                bool inOpen = false;
                for (size_t i = 0; i < open.size(); ++i) {
                    if (open[i] == nIdx) {
                        inOpen = true;
                        break;
                    }
                }
                if (!inOpen) {
                    open.push_back(nIdx);
                }
            }
        }
    }

    // No path found — return empty
    return result;
}

std::vector<Vector3> Pathfinding::smoothPath(const std::vector<Vector3>& path, const uint8_t* gridData) {
    if (path.size() <= 2) return path;
    if (!gridData) return path;

    std::vector<Vector3> smoothed;
    smoothed.push_back(path[0]);

    size_t current = 0;
    while (current < path.size() - 1) {
        size_t farthest = current + 1;

        // Try to skip as many intermediate waypoints as possible
        // by checking line of sight from the last added point
        for (size_t test = path.size() - 1; test > current + 1; --test) {
            if (hasLineOfSight(smoothed.back(), path[test], gridData)) {
                farthest = test;
                break;
            }
        }

        smoothed.push_back(path[farthest]);
        current = farthest;
    }

    return smoothed;
}

bool Pathfinding::hasLineOfSight(Vector3 from, Vector3 to, const uint8_t* gridData) {
    if (!gridData) return false;

    int x0 = worldToGridX(from.x);
    int z0 = worldToGridZ(from.z);
    int x1 = worldToGridX(to.x);
    int z1 = worldToGridZ(to.z);

    // Bresenham line algorithm through grid cells
    int dx = std::abs(x1 - x0);
    int dz = std::abs(z1 - z0);
    int sx = (x0 < x1) ? 1 : -1;
    int sz = (z0 < z1) ? 1 : -1;
    int err = dx - dz;

    int cx = x0;
    int cz = z0;

    while (true) {
        // Check if the current cell is a wall
        if (cx < 0 || cx >= MAP_W || cz < 0 || cz >= MAP_H) return false;
        if (!isValidCell(cx, cz, gridData)) return false;

        // Reached the target cell — sight is clear
        if (cx == x1 && cz == z1) break;

        int e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            cx += sx;
        }
        if (e2 < dx) {
            err += dx;
            cz += sz;
        }
    }

    return true;
}
