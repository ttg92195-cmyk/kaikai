#pragma once
#include "raylib.h"
#include <vector>
#include <random>
#include <cstdint>

struct SpawnedItem {
    enum Type { KEY, BATTERY, NOTE, HEALTH_PACK };
    Type type;
    Vector3 position;
    uint32_t id;
    bool active = true;
};

class ItemSpawnSystem {
public:
    ItemSpawnSystem();
    ~ItemSpawnSystem() = default;
    
    void initialize(const uint8_t* gridData); // spawn items at game start
    void update(float deltaTime);
    
    const std::vector<SpawnedItem>& getItems() const;
    SpawnedItem* getItem(uint32_t id);
    
    void pickUp(uint32_t itemId, uint32_t playerId);
    void respawnItem(uint32_t itemId); // for items that should respawn
    
    // Random spawning
    Vector3 getRandomFloorPosition(const uint8_t* gridData) const;
    
    // Rendering
    void render() const;
    
private:
    std::vector<SpawnedItem> items;
    std::vector<Vector3> spawnPositions; // valid floor positions
    std::mt19937 rng;
    uint32_t nextItemId = 1;
    
    // Spawn counts
    static constexpr int KEY_COUNT = 3;
    static constexpr int BATTERY_COUNT = 5;
    static constexpr int NOTE_COUNT = 4;
    static constexpr int HEALTH_PACK_COUNT = 2;
    
    void collectValidPositions(const uint8_t* gridData);
    void spawnItems();
    void renderItem(const SpawnedItem& item) const; // render individual item with raylib
};
