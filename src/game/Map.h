#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "raylib.h"

struct Door {
    Vector3 position = {0, 0, 0};
    bool isOpen = false;
    uint32_t id = 0;
    bool requiresKey = false;
    bool isLocked = true;
};

struct Switch {
    Vector3 position = {0, 0, 0};
    bool isPressed = false;
    uint32_t linkedDoorId = 0;
    uint32_t id = 0;
    bool requiresTwoPlayers = false;
    float pressTimer = 0.0f;
};

struct Item {
    enum Type { KEY, BATTERY, NOTE, HEALTH_PACK };
    Type type = KEY;
    Vector3 position = {0, 0, 0};
    uint32_t id = 0;
    bool isPickedUp = false;
};

class GameMap {
public:
    GameMap();
    ~GameMap() = default;

    void generate(); // procedural horror map
    void render() const;

    const uint8_t* getGridData() const;
    bool isWall(int gx, int gz) const;
    bool isWall(float worldX, float worldZ) const;

    // Doors
    std::vector<Door>& getDoors();
    const std::vector<Door>& getDoors() const;
    Door* getDoor(uint32_t id);
    void toggleDoor(uint32_t id);
    void openDoor(uint32_t id);
    void closeDoor(uint32_t id);

    // Switches
    std::vector<Switch>& getSwitches();
    const std::vector<Switch>& getSwitches() const;
    Switch* getSwitch(uint32_t id);
    void pressSwitch(uint32_t id, uint32_t playerId);
    void releaseSwitch(uint32_t id);

    // Items
    std::vector<Item>& getItems();
    const std::vector<Item>& getItems() const;
    Item* getItem(uint32_t id);
    void pickUpItem(uint32_t id, uint32_t playerId);
    void spawnItemAt(Item::Type type, Vector3 pos);

    // Spawn points
    Vector3 getRandomSpawnPoint() const;
    Vector3 getGhostSpawnPoint() const;

    // Rendering helpers
    void renderFloor() const;
    void renderCeiling() const;
    void renderWalls() const;
    void renderDoors() const;
    void renderSwitches() const;
    void renderItems() const;

private:
    uint8_t grid[50][50]; // 0=floor, 1=wall, 2=door, 3=switch
    std::vector<Door> doors;
    std::vector<Switch> switches;
    std::vector<Item> items;
    std::vector<Vector3> spawnPoints;
    Vector3 ghostSpawn;

    // Internal room representation for generation
    struct Room {
        int x, z;      // top-left grid position
        int width, height;
        int centerX, centerZ;
    };

    std::vector<Room> rooms;

    // Generation steps
    void generateRooms();
    void generateCorridors();
    void placeDoors();
    void placeSwitches();
    void placeItems();

    // Rendering helpers
    void renderWallTile(int gx, int gz) const;

    // ID counters
    uint32_t nextDoorId = 1;
    uint32_t nextSwitchId = 1;
    uint32_t nextItemId = 1;

    // Map dimensions
    static constexpr int MAP_WIDTH = 50;
    static constexpr int MAP_HEIGHT = 50;
    static constexpr float TILE_SIZE = 2.0f;

    // Generation parameters
    static constexpr int MIN_ROOM_SIZE = 4;
    static constexpr int MAX_ROOM_SIZE = 10;
    static constexpr int ROOM_ATTEMPTS = 40;
    static constexpr int MIN_ROOMS = 8;
    static constexpr float WALL_HEIGHT = 3.0f;
};
