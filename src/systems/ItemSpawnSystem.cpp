#include "ItemSpawnSystem.h"
#include "../utils/Constants.h"
#include <algorithm>
#include <cmath>

using namespace Kaikai;

ItemSpawnSystem::ItemSpawnSystem()
    : rng(std::random_device{}())
{
}

void ItemSpawnSystem::initialize(const uint8_t* gridData) {
    // Collect all valid floor positions from the grid data
    collectValidPositions(gridData);

    // Spawn items at random positions from the valid set
    spawnItems();
}

void ItemSpawnSystem::update(float deltaTime) {
    // Animate item bobbing and particle effects
    // The actual animation is done in render(), but we can update
    // any state here (e.g., respawn timers for picked-up items)
    for (auto& item : items) {
        // Items that are inactive but have respawn logic could be handled here
        // Currently, items stay inactive after pickup unless respawnItem() is called
    }
}

const std::vector<SpawnedItem>& ItemSpawnSystem::getItems() const {
    return items;
}

SpawnedItem* ItemSpawnSystem::getItem(uint32_t id) {
    for (auto& item : items) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

void ItemSpawnSystem::pickUp(uint32_t itemId, uint32_t playerId) {
    SpawnedItem* item = getItem(itemId);
    if (item && item->active) {
        item->active = false;
        // Network sync would be triggered here in a full implementation
        // e.g., networkBroadcastItemPickup(itemId, playerId);
    }
}

void ItemSpawnSystem::respawnItem(uint32_t itemId) {
    SpawnedItem* item = getItem(itemId);
    if (item && !item->active) {
        // Respawn at a new random position
        item->position = getRandomFloorPosition(nullptr);
        item->active = true;
    }
}

Vector3 ItemSpawnSystem::getRandomFloorPosition(const uint8_t* gridData) const {
    if (spawnPositions.empty()) {
        // Fallback: return center of world if no valid positions
        return { MAP_WIDTH * TILE_SIZE * 0.5f, 0.0f, MAP_HEIGHT * TILE_SIZE * 0.5f };
    }

    // Use const_cast to allow rng usage in const method
    // (rng mutation is a logical const operation for position selection)
    std::uniform_int_distribution<size_t> dist(0, spawnPositions.size() - 1);
    size_t idx = dist(const_cast<std::mt19937&>(rng));
    return spawnPositions[idx];
}

#if !defined(KAIKAI_HEADLESS)
void ItemSpawnSystem::render() const {
    for (const auto& item : items) {
        if (item.active) {
            renderItem(item);
        }
    }
}
#endif

void ItemSpawnSystem::collectValidPositions(const uint8_t* gridData) {
    spawnPositions.clear();

    if (!gridData) {
        // If no grid data provided, generate a default set of positions
        // covering the entire grid area as walkable
        for (int z = 0; z < MAP_HEIGHT; z++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                Vector3 pos = {
                    x * TILE_SIZE + TILE_SIZE * 0.5f,
                    0.0f,
                    z * TILE_SIZE + TILE_SIZE * 0.5f
                };
                spawnPositions.push_back(pos);
            }
        }
        return;
    }

    // Grid cell values: 0 = wall, 1 = floor, 2 = dark floor, 3 = story area
    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            uint8_t cell = gridData[z * MAP_WIDTH + x];
            // Floor cells (1, 2, 3) are valid spawn positions
            if (cell >= 1 && cell <= 3) {
                Vector3 pos = {
                    x * TILE_SIZE + TILE_SIZE * 0.5f,
                    0.0f,
                    z * TILE_SIZE + TILE_SIZE * 0.5f
                };
                spawnPositions.push_back(pos);
            }
        }
    }
}

void ItemSpawnSystem::spawnItems() {
    if (spawnPositions.empty()) {
        return;
    }

    // Shuffle positions to randomize item placement
    std::vector<Vector3> shuffledPositions = spawnPositions;
    std::shuffle(shuffledPositions.begin(), shuffledPositions.end(), rng);

    size_t posIndex = 0;

    // Helper lambda to get next position and advance the index
    auto nextPos = [&]() -> Vector3 {
        if (posIndex >= shuffledPositions.size()) {
            posIndex = 0;
            std::shuffle(shuffledPositions.begin(), shuffledPositions.end(), rng);
        }
        return shuffledPositions[posIndex++];
    };

    // === Spawn Keys ===
    // Keys are spread out with minimum distance between them
    std::vector<Vector3> keyPositions;
    float minKeyDistance = 15.0f; // Minimum distance between keys

    int keysSpawned = 0;
    int keyAttempts = 0;
    while (keysSpawned < KEY_COUNT && keyAttempts < 500) {
        Vector3 candidate = nextPos();
        bool tooClose = false;

        for (const auto& existingPos : keyPositions) {
            float dx = candidate.x - existingPos.x;
            float dz = candidate.z - existingPos.z;
            float dist = sqrtf(dx * dx + dz * dz);
            if (dist < minKeyDistance) {
                tooClose = true;
                break;
            }
        }

        if (!tooClose) {
            SpawnedItem item;
            item.type = SpawnedItem::KEY;
            item.position = candidate;
            item.id = nextItemId++;
            item.active = true;
            items.push_back(item);
            keyPositions.push_back(candidate);
            keysSpawned++;
        }
        keyAttempts++;
    }

    // === Spawn Batteries ===
    // Batteries are more common in darker areas (grid cell value 2)
    // First, collect dark positions
    std::vector<Vector3> darkPositions;
    for (const auto& pos : shuffledPositions) {
        // Estimate grid cell from position - dark cells are type 2
        int gx = (int)(pos.x / TILE_SIZE);
        int gz = (int)(pos.z / TILE_SIZE);
        // Heuristic: positions in the outer thirds of the map are "darker"
        bool inDarkArea = (gx < MAP_WIDTH / 3 || gx > MAP_WIDTH * 2 / 3 ||
                           gz < MAP_HEIGHT / 3 || gz > MAP_HEIGHT * 2 / 3);
        if (inDarkArea) {
            darkPositions.push_back(pos);
        }
    }

    // Spawn 60% of batteries in dark areas, 40% anywhere
    int darkBatteries = (int)(BATTERY_COUNT * 0.6f);
    int normalBatteries = BATTERY_COUNT - darkBatteries;

    // Dark area batteries
    for (int i = 0; i < darkBatteries && !darkPositions.empty(); i++) {
        std::uniform_int_distribution<size_t> dist(0, darkPositions.size() - 1);
        size_t idx = dist(rng);
        SpawnedItem item;
        item.type = SpawnedItem::BATTERY;
        item.position = darkPositions[idx];
        item.id = nextItemId++;
        item.active = true;
        items.push_back(item);
    }

    // Normal batteries
    for (int i = 0; i < normalBatteries; i++) {
        SpawnedItem item;
        item.type = SpawnedItem::BATTERY;
        item.position = nextPos();
        item.id = nextItemId++;
        item.active = true;
        items.push_back(item);
    }

    // === Spawn Notes ===
    // Notes are placed near story-relevant locations (grid cell value 3)
    std::vector<Vector3> storyPositions;
    for (const auto& pos : shuffledPositions) {
        int gx = (int)(pos.x / TILE_SIZE);
        int gz = (int)(pos.z / TILE_SIZE);
        // Story areas are typically in the middle region
        bool inStoryArea = (gx > MAP_WIDTH / 4 && gx < MAP_WIDTH * 3 / 4 &&
                            gz > MAP_HEIGHT / 4 && gz < MAP_HEIGHT * 3 / 4);
        if (inStoryArea) {
            storyPositions.push_back(pos);
        }
    }

    int notesSpawned = 0;
    // Try to place notes in story areas first
    for (int i = 0; i < NOTE_COUNT && !storyPositions.empty(); i++) {
        std::uniform_int_distribution<size_t> dist(0, storyPositions.size() - 1);
        size_t idx = dist(rng);
        SpawnedItem item;
        item.type = SpawnedItem::NOTE;
        item.position = storyPositions[idx];
        item.id = nextItemId++;
        item.active = true;
        items.push_back(item);
        notesSpawned++;
    }

    // Fill remaining notes at random positions
    for (int i = notesSpawned; i < NOTE_COUNT; i++) {
        SpawnedItem item;
        item.type = SpawnedItem::NOTE;
        item.position = nextPos();
        item.id = nextItemId++;
        item.active = true;
        items.push_back(item);
    }

    // === Spawn Health Packs ===
    // Health packs are rare and placed somewhat centrally
    for (int i = 0; i < HEALTH_PACK_COUNT; i++) {
        SpawnedItem item;
        item.type = SpawnedItem::HEALTH_PACK;
        item.position = nextPos();
        item.id = nextItemId++;
        item.active = true;
        items.push_back(item);
    }
}

#if !defined(KAIKAI_HEADLESS)
void ItemSpawnSystem::renderItem(const SpawnedItem& item) const {
    float time = (float)GetTime();
    // Common animation: bobbing up and down
    float bobOffset = sinf(time * 2.0f + item.position.x * 0.5f + item.position.z * 0.7f) * 0.1f;
    Vector3 renderPos = item.position;
    renderPos.y += 0.5f + bobOffset; // Float above ground

    // Common animation: slow rotation
    float rotation = time * 1.5f;

    // Glow effect: a subtle halo around the item
    Color glowColor;

    switch (item.type) {
        case SpawnedItem::KEY: {
            glowColor = { 255, 220, 50, 40 };

            // Draw glow
            DrawSphere(renderPos, 0.25f, glowColor);

            // Key shaft: small yellow cylinder
            Vector3 shaftStart = renderPos;
            shaftStart.x -= 0.06f;
            Vector3 shaftEnd = renderPos;
            shaftEnd.x += 0.12f;
            DrawCylinderEx(shaftStart, shaftEnd, 0.02f, 0.02f, 6, YELLOW);

            // Key ring (torus approximation): draw small spheres in a ring
            for (int i = 0; i < 12; i++) {
                float angle = (float)i / 12.0f * 6.2832f;
                Vector3 ringPos = renderPos;
                ringPos.x -= 0.06f;
                ringPos.y += cosf(angle) * 0.05f;
                ringPos.z += sinf(angle) * 0.05f;
                DrawSphere(ringPos, 0.012f, GOLD);
            }

            // Key teeth: small notches at the end
            for (int i = 0; i < 3; i++) {
                Vector3 toothPos = renderPos;
                toothPos.x += 0.04f + i * 0.03f;
                toothPos.y -= 0.03f;
                DrawCube(toothPos, 0.015f, 0.04f, 0.03f, GOLD);
            }
            break;
        }

        case SpawnedItem::BATTERY: {
            glowColor = { 50, 255, 50, 35 };

            // Draw glow
            DrawSphere(renderPos, 0.2f, glowColor);

            // Battery body: small green rectangle
            DrawCube(renderPos, 0.06f, 0.12f, 0.03f, DARKGREEN);

            // Battery terminal: small cap on top
            Vector3 terminalPos = renderPos;
            terminalPos.y += 0.07f;
            DrawCube(terminalPos, 0.03f, 0.02f, 0.03f, LIME);

            // Battery charge indicator: small green strip
            float chargePulse = 0.7f + sinf(time * 3.0f) * 0.3f;
            Vector3 chargePos = renderPos;
            chargePos.z += 0.016f;
            Color chargeColor = { 0, (unsigned char)(255 * chargePulse), 0, 255 };
            DrawCube(chargePos, 0.04f, 0.08f, 0.001f, chargeColor);
            break;
        }

        case SpawnedItem::NOTE: {
            glowColor = { 255, 255, 255, 25 };

            // Draw glow
            DrawSphere(renderPos, 0.2f, glowColor);

            // Note: white plane with slight float animation
            // Use a thin cube to represent the paper
            float tiltAngle = sinf(time * 0.8f) * 0.1f;
            Vector3 notePos = renderPos;
            notePos.y += sinf(time * 1.5f) * 0.02f;

            // Paper body
            DrawCube(notePos, 0.15f, 0.002f, 0.1f, WHITE);

            // Text lines on the note (small dark rectangles)
            for (int i = 0; i < 4; i++) {
                Vector3 linePos = notePos;
                linePos.y += 0.002f;
                linePos.z -= 0.03f + i * 0.018f;
                float lineLen = 0.05f + sinf(i * 2.3f) * 0.02f;
                DrawCube(linePos, lineLen, 0.001f, 0.004f, { 40, 40, 40, 200 });
            }

            break;
        }

        case SpawnedItem::HEALTH_PACK: {
            glowColor = { 255, 50, 50, 30 };

            // Draw glow
            DrawSphere(renderPos, 0.25f, glowColor);

            // Health pack: white box with red cross
            DrawCube(renderPos, 0.12f, 0.08f, 0.12f, WHITE);

            // Red cross - horizontal bar
            Vector3 crossH = renderPos;
            crossH.y += 0.041f;
            DrawCube(crossH, 0.08f, 0.002f, 0.025f, RED);

            // Red cross - vertical bar
            Vector3 crossV = renderPos;
            crossV.y += 0.041f;
            DrawCube(crossV, 0.025f, 0.002f, 0.08f, RED);

            // Side cross for visibility from other angles
            Vector3 sideCrossH = renderPos;
            sideCrossH.x += 0.061f;
            DrawCube(sideCrossH, 0.002f, 0.06f, 0.02f, RED);

            Vector3 sideCrossV = renderPos;
            sideCrossV.x += 0.061f;
            DrawCube(sideCrossV, 0.002f, 0.02f, 0.06f, RED);

            break;
        }
    }

    // === Subtle particle effect: small floating dots ===
    for (int i = 0; i < 4; i++) {
        float particleTime = time * 0.8f + i * 1.57f;
        float particleRadius = 0.15f + sinf(particleTime * 1.2f) * 0.08f;
        float particleAngle = particleTime + i * 1.57f;
        float particleY = sinf(particleTime * 2.0f) * 0.15f;

        Vector3 particlePos = item.position;
        particlePos.x += cosf(particleAngle) * particleRadius;
        particlePos.y += 0.5f + particleY;
        particlePos.z += sinf(particleAngle) * particleRadius;

        // Particle color matches item type
        Color particleColor;
        switch (item.type) {
            case SpawnedItem::KEY:         particleColor = { 255, 220, 80, 150 }; break;
            case SpawnedItem::BATTERY:     particleColor = { 80, 255, 80, 150 }; break;
            case SpawnedItem::NOTE:        particleColor = { 220, 220, 255, 130 }; break;
            case SpawnedItem::HEALTH_PACK: particleColor = { 255, 80, 80, 150 }; break;
        }

        // Fade particle based on vertical position
        float fade = 1.0f - (particleY + 0.15f) / 0.3f;
        fade = std::clamp(fade, 0.0f, 1.0f);
        particleColor.a = (unsigned char)(particleColor.a * fade);

        DrawSphere(particlePos, 0.015f, particleColor);
    }
}
#endif // !KAIKAI_HEADLESS