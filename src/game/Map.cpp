#include "Map.h"
#if !defined(KAIKAI_HEADLESS)
#include "raymath.h"
#endif
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstring>

// Global map grid shared with Player collision system
uint8_t g_mapGrid[50][50];

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

GameMap::GameMap() {
    memset(grid, 1, sizeof(grid)); // Start fully filled with walls
    ghostSpawn = {TILE_SIZE * 2.0f, 0.0f, TILE_SIZE * 2.0f};
}

// ---------------------------------------------------------------------------
// Public generation entry point
// ---------------------------------------------------------------------------

void GameMap::generate() {
    // Reset everything
    memset(grid, 1, sizeof(grid));
    rooms.clear();
    doors.clear();
    switches.clear();
    items.clear();
    spawnPoints.clear();
    nextDoorId = 1;
    nextSwitchId = 1;
    nextItemId = 1;

    // Seed the RNG
    srand(static_cast<unsigned>(time(nullptr)));

    generateRooms();
    generateCorridors();
    placeDoors();
    placeSwitches();
    placeItems();

    // Copy to global grid for collision
    memcpy(g_mapGrid, grid, sizeof(grid));
}

// ---------------------------------------------------------------------------
// Room generation – random placement with overlap rejection
// ---------------------------------------------------------------------------

void GameMap::generateRooms() {
    int roomsGenerated = 0;

    for (int attempt = 0; attempt < ROOM_ATTEMPTS && roomsGenerated < 20; ++attempt) {
        int w = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE + 1));
        int h = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE + 1));
        int x = 2 + (rand() % (MAP_WIDTH - w - 4));
        int z = 2 + (rand() % (MAP_HEIGHT - h - 4));

        // Check overlap with existing rooms (with 1-tile padding)
        bool overlaps = false;
        for (const auto& r : rooms) {
            if (x - 1 < r.x + r.width && x + w + 1 > r.x &&
                z - 1 < r.z + r.height && z + h + 1 > r.z) {
                overlaps = true;
                break;
            }
        }

        if (overlaps) continue;

        // Carve the room into the grid
        Room room;
        room.x = x;
        room.z = z;
        room.width = w;
        room.height = h;
        room.centerX = x + w / 2;
        room.centerZ = z + h / 2;

        for (int rz = z; rz < z + h; ++rz) {
            for (int rx = x; rx < x + w; ++rx) {
                grid[rz][rx] = 0; // floor
            }
        }

        rooms.push_back(room);
        roomsGenerated++;
    }

    // Ensure we have at least MIN_ROOMS
    // If not enough rooms were placed, add smaller rooms in the remaining space
    while (roomsGenerated < MIN_ROOMS) {
        int w = MIN_ROOM_SIZE;
        int h = MIN_ROOM_SIZE;
        int x = 2 + (rand() % (MAP_WIDTH - w - 4));
        int z = 2 + (rand() % (MAP_HEIGHT - h - 4));

        // Force place even with slight overlap (carve anyway)
        bool hasOverlap = false;
        for (const auto& r : rooms) {
            if (x < r.x + r.width && x + w > r.x &&
                z < r.z + r.height && z + h > r.z) {
                hasOverlap = true;
                break;
            }
        }
        if (hasOverlap) continue;

        Room room;
        room.x = x;
        room.z = z;
        room.width = w;
        room.height = h;
        room.centerX = x + w / 2;
        room.centerZ = z + h / 2;

        for (int rz = z; rz < z + h; ++rz) {
            for (int rx = x; rx < x + w; ++rx) {
                grid[rz][rx] = 0;
            }
        }

        rooms.push_back(room);
        roomsGenerated++;
    }

    // Set spawn points at room centers
    for (size_t i = 0; i < rooms.size(); ++i) {
        Vector3 sp = {
            rooms[i].centerX * TILE_SIZE + TILE_SIZE * 0.5f,
            0.0f,
            rooms[i].centerZ * TILE_SIZE + TILE_SIZE * 0.5f
        };
        spawnPoints.push_back(sp);
    }

    // Ghost spawns at the center of the farthest room from room 0
    float maxDist = -1.0f;
    int ghostRoom = 0;
    if (rooms.size() > 1) {
        float baseX = rooms[0].centerX * TILE_SIZE;
        float baseZ = rooms[0].centerZ * TILE_SIZE;
        for (size_t i = 1; i < rooms.size(); ++i) {
            float dx = rooms[i].centerX * TILE_SIZE - baseX;
            float dz = rooms[i].centerZ * TILE_SIZE - baseZ;
            float dist = dx * dx + dz * dz;
            if (dist > maxDist) {
                maxDist = dist;
                ghostRoom = static_cast<int>(i);
            }
        }
    }
    ghostSpawn = {
        rooms[ghostRoom].centerX * TILE_SIZE + TILE_SIZE * 0.5f,
        0.0f,
        rooms[ghostRoom].centerZ * TILE_SIZE + TILE_SIZE * 0.5f
    };
}

// ---------------------------------------------------------------------------
// Corridor generation – connect each room to its nearest unconnected room
// ---------------------------------------------------------------------------

void GameMap::generateCorridors() {
    if (rooms.size() < 2) return;

    // Simple approach: connect each room to the next one in the list
    // using L-shaped corridors (horizontal then vertical or vice versa)
    for (size_t i = 0; i < rooms.size() - 1; ++i) {
        int cx1 = rooms[i].centerX;
        int cz1 = rooms[i].centerZ;
        int cx2 = rooms[i + 1].centerX;
        int cz2 = rooms[i + 1].centerZ;

        // Randomly decide horizontal-first or vertical-first
        if (rand() % 2 == 0) {
            // Horizontal first
            int stepX = (cx2 > cx1) ? 1 : -1;
            for (int x = cx1; x != cx2; x += stepX) {
                grid[cz1][x] = 0;
                // Make corridor 2 tiles wide
                if (cz1 + 1 < MAP_HEIGHT) grid[cz1 + 1][x] = 0;
            }
            int stepZ = (cz2 > cz1) ? 1 : -1;
            for (int z = cz1; z != cz2; z += stepZ) {
                grid[z][cx2] = 0;
                if (cx2 + 1 < MAP_WIDTH) grid[z][cx2 + 1] = 0;
            }
        } else {
            // Vertical first
            int stepZ = (cz2 > cz1) ? 1 : -1;
            for (int z = cz1; z != cz2; z += stepZ) {
                grid[z][cx1] = 0;
                if (cx1 + 1 < MAP_WIDTH) grid[z][cx1 + 1] = 0;
            }
            int stepX = (cx2 > cx1) ? 1 : -1;
            for (int x = cx1; x != cx2; x += stepX) {
                grid[cz2][x] = 0;
                if (cz2 + 1 < MAP_HEIGHT) grid[cz2 + 1][x] = 0;
            }
        }
    }

    // Also connect the last room back to the first for a loop
    if (rooms.size() > 2) {
        int cx1 = rooms[rooms.size() - 1].centerX;
        int cz1 = rooms[rooms.size() - 1].centerZ;
        int cx2 = rooms[0].centerX;
        int cz2 = rooms[0].centerZ;

        int stepX = (cx2 > cx1) ? 1 : -1;
        for (int x = cx1; x != cx2; x += stepX) {
            grid[cz1][x] = 0;
            if (cz1 + 1 < MAP_HEIGHT) grid[cz1 + 1][x] = 0;
        }
        int stepZ = (cz2 > cz1) ? 1 : -1;
        for (int z = cz1; z != cz2; z += stepZ) {
            grid[z][cx2] = 0;
            if (cx2 + 1 < MAP_WIDTH) grid[z][cx2 + 1] = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Door placement – at corridor-room junctions
// ---------------------------------------------------------------------------

void GameMap::placeDoors() {
    for (size_t i = 0; i < rooms.size(); ++i) {
        int rx = rooms[i].x;
        int rz = rooms[i].z;
        int rw = rooms[i].width;
        int rh = rooms[i].height;

        // Check each edge tile of the room for a corridor entrance
        // Top edge
        for (int x = rx + 1; x < rx + rw - 1; ++x) {
            if (rz > 0 && grid[rz - 1][x] == 0 && grid[rz][x] == 0) {
                Door d;
                d.id = nextDoorId++;
                d.position = {x * TILE_SIZE + TILE_SIZE * 0.5f, 0.0f,
                              rz * TILE_SIZE + TILE_SIZE * 0.5f};
                d.isOpen = false;
                d.requiresKey = (rand() % 4 == 0); // 25% chance of locked door
                d.isLocked = d.requiresKey;
                grid[rz][x] = 2; // mark as door tile
                doors.push_back(d);
            }
        }
        // Bottom edge
        for (int x = rx + 1; x < rx + rw - 1; ++x) {
            if (rz + rh < MAP_HEIGHT && grid[rz + rh][x] == 0 && grid[rz + rh - 1][x] == 0) {
                // Only place if not already a door
                if (grid[rz + rh - 1][x] != 2) {
                    Door d;
                    d.id = nextDoorId++;
                    d.position = {x * TILE_SIZE + TILE_SIZE * 0.5f, 0.0f,
                                  (rz + rh - 1) * TILE_SIZE + TILE_SIZE * 0.5f};
                    d.isOpen = false;
                    d.requiresKey = (rand() % 4 == 0);
                    d.isLocked = d.requiresKey;
                    grid[rz + rh - 1][x] = 2;
                    doors.push_back(d);
                }
            }
        }
        // Left edge
        for (int z = rz + 1; z < rz + rh - 1; ++z) {
            if (rx > 0 && grid[z][rx - 1] == 0 && grid[z][rx] == 0) {
                if (grid[z][rx] != 2) {
                    Door d;
                    d.id = nextDoorId++;
                    d.position = {rx * TILE_SIZE + TILE_SIZE * 0.5f, 0.0f,
                                  z * TILE_SIZE + TILE_SIZE * 0.5f};
                    d.isOpen = false;
                    d.requiresKey = (rand() % 4 == 0);
                    d.isLocked = d.requiresKey;
                    grid[z][rx] = 2;
                    doors.push_back(d);
                }
            }
        }
        // Right edge
        for (int z = rz + 1; z < rz + rh - 1; ++z) {
            if (rx + rw < MAP_WIDTH && grid[z][rx + rw] == 0 && grid[z][rx + rw - 1] == 0) {
                if (grid[z][rx + rw - 1] != 2) {
                    Door d;
                    d.id = nextDoorId++;
                    d.position = {(rx + rw - 1) * TILE_SIZE + TILE_SIZE * 0.5f, 0.0f,
                                  z * TILE_SIZE + TILE_SIZE * 0.5f};
                    d.isOpen = false;
                    d.requiresKey = (rand() % 4 == 0);
                    d.isLocked = d.requiresKey;
                    grid[z][rx + rw - 1] = 2;
                    doors.push_back(d);
                }
            }
        }
    }

    // Ensure at least one locked door for puzzle mechanic
    if (!doors.empty()) {
        bool hasLocked = false;
        for (const auto& d : doors) {
            if (d.isLocked) { hasLocked = true; break; }
        }
        if (!hasLocked) {
            doors[0].requiresKey = true;
            doors[0].isLocked = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Switch placement – in rooms, linked to locked doors
// ---------------------------------------------------------------------------

void GameMap::placeSwitches() {
    // For each locked door, place a switch in a different room
    uint32_t switchRoomIdx = 1; // Start from second room to avoid spawn room

    for (auto& door : doors) {
        if (!door.requiresKey) continue;

        if (switchRoomIdx >= rooms.size()) switchRoomIdx = 0;

        const Room& room = rooms[switchRoomIdx];

        Switch sw;
        sw.id = nextSwitchId++;
        sw.linkedDoorId = door.id;
        sw.requiresTwoPlayers = (rand() % 3 == 0); // 33% chance needs 2 players
        sw.isPressed = false;
        sw.pressTimer = 0.0f;

        // Place switch along a wall inside the room
        int sx = room.x + 1;
        int sz = room.z + 1;
        sw.position = {sx * TILE_SIZE + TILE_SIZE * 0.5f, 0.0f,
                       sz * TILE_SIZE + TILE_SIZE * 0.5f};

        grid[sz][sx] = 3; // mark as switch tile
        switches.push_back(sw);

        switchRoomIdx = (switchRoomIdx + 1) % rooms.size();
    }

    // Ensure at least one switch requires two players if there are locked doors
    if (!switches.empty()) {
        bool hasTwoPlayer = false;
        for (const auto& sw : switches) {
            if (sw.requiresTwoPlayers) { hasTwoPlayer = true; break; }
        }
        if (!hasTwoPlayer && switches.size() > 0) {
            switches[0].requiresTwoPlayers = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Item placement – scatter items throughout rooms
// ---------------------------------------------------------------------------

void GameMap::placeItems() {
    // Place keys for locked doors (near their switches)
    for (const auto& sw : switches) {
        // Place key in a different room than the switch
        uint32_t keyRoom = 0;
        for (size_t i = 0; i < rooms.size(); ++i) {
            // Find a room that doesn't contain this switch
            int sx = static_cast<int>(sw.position.x / TILE_SIZE);
            int sz = static_cast<int>(sw.position.z / TILE_SIZE);
            bool inThisRoom = (sx >= rooms[i].x && sx < rooms[i].x + rooms[i].width &&
                               sz >= rooms[i].z && sz < rooms[i].z + rooms[i].height);
            if (!inThisRoom) {
                keyRoom = static_cast<uint32_t>(i);
                break;
            }
        }

        const Room& room = rooms[keyRoom];
        Item key;
        key.id = nextItemId++;
        key.type = Item::KEY;
        key.isPickedUp = false;
        key.position = {
            (room.x + room.width / 2) * TILE_SIZE + TILE_SIZE * 0.5f,
            0.5f,
            (room.z + room.height / 2) * TILE_SIZE + TILE_SIZE * 0.5f
        };
        items.push_back(key);
    }

    // Place batteries in random rooms
    int numBatteries = 3 + (rand() % 4);
    for (int i = 0; i < numBatteries; ++i) {
        int roomIdx = rand() % rooms.size();
        const Room& room = rooms[roomIdx];

        Item bat;
        bat.id = nextItemId++;
        bat.type = Item::BATTERY;
        bat.isPickedUp = false;
        bat.position = {
            (room.x + 1 + rand() % (room.width - 2)) * TILE_SIZE + TILE_SIZE * 0.5f,
            0.5f,
            (room.z + 1 + rand() % (room.height - 2)) * TILE_SIZE + TILE_SIZE * 0.5f
        };
        items.push_back(bat);
    }

    // Place health packs (restore sanity)
    int numHealth = 2 + (rand() % 3);
    for (int i = 0; i < numHealth; ++i) {
        int roomIdx = rand() % rooms.size();
        const Room& room = rooms[roomIdx];

        Item hp;
        hp.id = nextItemId++;
        hp.type = Item::HEALTH_PACK;
        hp.isPickedUp = false;
        hp.position = {
            (room.x + 1 + rand() % (room.width - 2)) * TILE_SIZE + TILE_SIZE * 0.5f,
            0.5f,
            (room.z + 1 + rand() % (room.height - 2)) * TILE_SIZE + TILE_SIZE * 0.5f
        };
        items.push_back(hp);
    }

    // Place notes (lore / clues)
    int numNotes = 2 + (rand() % 2);
    for (int i = 0; i < numNotes; ++i) {
        int roomIdx = rand() % rooms.size();
        const Room& room = rooms[roomIdx];

        Item note;
        note.id = nextItemId++;
        note.type = Item::NOTE;
        note.isPickedUp = false;
        note.position = {
            (room.x + 1 + rand() % (room.width - 2)) * TILE_SIZE + TILE_SIZE * 0.5f,
            0.5f,
            (room.z + 1 + rand() % (room.height - 2)) * TILE_SIZE + TILE_SIZE * 0.5f
        };
        items.push_back(note);
    }
}

// ---------------------------------------------------------------------------
// Grid queries
// ---------------------------------------------------------------------------

const uint8_t* GameMap::getGridData() const {
    return &grid[0][0];
}

bool GameMap::isWall(int gx, int gz) const {
    if (gx < 0 || gx >= MAP_WIDTH || gz < 0 || gz >= MAP_HEIGHT) return true;
    return grid[gz][gx] == 1;
}

bool GameMap::isWall(float worldX, float worldZ) const {
    int gx = static_cast<int>(worldX / TILE_SIZE);
    int gz = static_cast<int>(worldZ / TILE_SIZE);
    return isWall(gx, gz);
}

// ---------------------------------------------------------------------------
// Door management
// ---------------------------------------------------------------------------

std::vector<Door>& GameMap::getDoors() { return doors; }
const std::vector<Door>& GameMap::getDoors() const { return doors; }

std::vector<Switch>& GameMap::getSwitches() { return switches; }
const std::vector<Switch>& GameMap::getSwitches() const { return switches; }

const std::vector<Item>& GameMap::getItems() const { return items; }

Door* GameMap::getDoor(uint32_t id) {
    for (auto& d : doors) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

void GameMap::toggleDoor(uint32_t id) {
    Door* d = getDoor(id);
    if (!d) return;
    if (d->isLocked) return; // can't toggle locked doors
    d->isOpen = !d->isOpen;

    // Update grid
    int gx = static_cast<int>(d->position.x / TILE_SIZE);
    int gz = static_cast<int>(d->position.z / TILE_SIZE);
    if (gx >= 0 && gx < MAP_WIDTH && gz >= 0 && gz < MAP_HEIGHT) {
        grid[gz][gx] = d->isOpen ? 0 : 2;
        g_mapGrid[gz][gx] = grid[gz][gx];
    }
}

void GameMap::openDoor(uint32_t id) {
    Door* d = getDoor(id);
    if (!d || d->isLocked) return;
    d->isOpen = true;

    int gx = static_cast<int>(d->position.x / TILE_SIZE);
    int gz = static_cast<int>(d->position.z / TILE_SIZE);
    if (gx >= 0 && gx < MAP_WIDTH && gz >= 0 && gz < MAP_HEIGHT) {
        grid[gz][gx] = 0;
        g_mapGrid[gz][gx] = 0;
    }
}

void GameMap::closeDoor(uint32_t id) {
    Door* d = getDoor(id);
    if (!d) return;
    d->isOpen = false;

    int gx = static_cast<int>(d->position.x / TILE_SIZE);
    int gz = static_cast<int>(d->position.z / TILE_SIZE);
    if (gx >= 0 && gx < MAP_WIDTH && gz >= 0 && gz < MAP_HEIGHT) {
        grid[gz][gx] = 2;
        g_mapGrid[gz][gx] = 2;
    }
}

// ---------------------------------------------------------------------------
// Switch management
// ---------------------------------------------------------------------------

Switch* GameMap::getSwitch(uint32_t id) {
    for (auto& s : switches) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

void GameMap::pressSwitch(uint32_t id, uint32_t playerId) {
    Switch* s = getSwitch(id);
    if (!s) return;
    s->isPressed = true;
#if defined(KAIKAI_HEADLESS)
    s->pressTimer += 1.0f / 60.0f; // approximate frame time
#else
    s->pressTimer += GetFrameTime();
#endif

    // If switch doesn't require holding, immediately unlock linked door
    if (!s->requiresTwoPlayers) {
        Door* d = getDoor(s->linkedDoorId);
        if (d && d->isLocked) {
            d->isLocked = false;
            openDoor(d->id);
        }
    }
    (void)playerId; // Used for two-player tracking in Game layer
}

void GameMap::releaseSwitch(uint32_t id) {
    Switch* s = getSwitch(id);
    if (!s) return;
    s->isPressed = false;
    s->pressTimer = 0.0f;
}

// ---------------------------------------------------------------------------
// Item management
// ---------------------------------------------------------------------------

std::vector<Item>& GameMap::getItems() {
    return items;
}

Item* GameMap::getItem(uint32_t id) {
    for (auto& it : items) {
        if (it.id == id) return &it;
    }
    return nullptr;
}

void GameMap::pickUpItem(uint32_t id, uint32_t playerId) {
    Item* it = getItem(id);
    if (!it || it->isPickedUp) return;
    it->isPickedUp = true;

    // The actual effect (battery refill, key collection, etc.) is handled
    // by the Game layer which calls this then applies the effect to the player.
    (void)playerId;
}

void GameMap::spawnItemAt(Item::Type type, Vector3 pos) {
    Item it;
    it.id = nextItemId++;
    it.type = type;
    it.position = pos;
    it.isPickedUp = false;
    items.push_back(it);
}

// ---------------------------------------------------------------------------
// Spawn points
// ---------------------------------------------------------------------------

Vector3 GameMap::getRandomSpawnPoint() const {
    if (spawnPoints.empty()) return {TILE_SIZE, 0.0f, TILE_SIZE};
    int idx = rand() % spawnPoints.size();
    return spawnPoints[idx];
}

Vector3 GameMap::getGhostSpawnPoint() const {
    return ghostSpawn;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void GameMap::render() const {
#if !defined(KAIKAI_HEADLESS)
    renderFloor();
    renderCeiling();
    renderWalls();
    renderDoors();
    renderSwitches();
    renderItems();
#endif
}

void GameMap::renderFloor() const {
#if !defined(KAIKAI_HEADLESS)
    for (int gz = 0; gz < MAP_HEIGHT; ++gz) {
        for (int gx = 0; gx < MAP_WIDTH; ++gx) {
            if (grid[gz][gx] != 1) { // not a wall
                Vector3 pos = {
                    gx * TILE_SIZE + TILE_SIZE * 0.5f,
                    0.0f,
                    gz * TILE_SIZE + TILE_SIZE * 0.5f
                };
                DrawCube(pos, TILE_SIZE, 0.05f, TILE_SIZE, DARKGRAY);
            }
        }
    }
#endif
}

void GameMap::renderCeiling() const {
#if !defined(KAIKAI_HEADLESS)
    for (int gz = 0; gz < MAP_HEIGHT; ++gz) {
        for (int gx = 0; gx < MAP_WIDTH; ++gx) {
            if (grid[gz][gx] != 1) {
                Vector3 pos = {
                    gx * TILE_SIZE + TILE_SIZE * 0.5f,
                    WALL_HEIGHT,
                    gz * TILE_SIZE + TILE_SIZE * 0.5f
                };
                DrawCube(pos, TILE_SIZE, 0.05f, TILE_SIZE, {30, 30, 35, 255});
            }
        }
    }
#endif
}

void GameMap::renderWalls() const {
#if !defined(KAIKAI_HEADLESS)
    for (int gz = 0; gz < MAP_HEIGHT; ++gz) {
        for (int gx = 0; gx < MAP_WIDTH; ++gx) {
            if (grid[gz][gx] == 1) {
                renderWallTile(gx, gz);
            }
        }
    }
#endif
}

void GameMap::renderWallTile(int gx, int gz) const {
#if !defined(KAIKAI_HEADLESS)
    Vector3 pos = {
        gx * TILE_SIZE + TILE_SIZE * 0.5f,
        WALL_HEIGHT * 0.5f,
        gz * TILE_SIZE + TILE_SIZE * 0.5f
    };

    // Dark horror-themed wall color with subtle variation
    Color wallColor = {45, 40, 50, 255};
    // Add slight color variation based on position for visual interest
    wallColor.r = static_cast<unsigned char>(45 + ((gx * 7 + gz * 13) % 10));
    wallColor.g = static_cast<unsigned char>(40 + ((gx * 11 + gz * 3) % 8));
    wallColor.b = static_cast<unsigned char>(50 + ((gx * 5 + gz * 9) % 12));

    DrawCube(pos, TILE_SIZE, WALL_HEIGHT, TILE_SIZE, wallColor);
#endif
}

void GameMap::renderDoors() const {
#if !defined(KAIKAI_HEADLESS)
    for (const auto& door : doors) {
        if (door.isOpen) continue; // Open doors are invisible (grid already floor)

        Vector3 pos = door.position;
        pos.y = WALL_HEIGHT * 0.5f;

        Color doorColor = door.isLocked ? MAROON : BROWN;

        // Determine door orientation based on surrounding walls
        int gx = static_cast<int>(door.position.x / TILE_SIZE);
        int gz = static_cast<int>(door.position.z / TILE_SIZE);

        bool wallAbove = (gz > 0 && grid[gz - 1][gx] == 1);
        bool wallBelow = (gz < MAP_HEIGHT - 1 && grid[gz + 1][gx] == 1);

        if (wallAbove || wallBelow) {
            // Door faces along X axis (wall is N-S)
            DrawCube(pos, TILE_SIZE, WALL_HEIGHT, 0.15f, doorColor);
        } else {
            // Door faces along Z axis (wall is E-W)
            DrawCube(pos, 0.15f, WALL_HEIGHT, TILE_SIZE, doorColor);
        }

        // Door frame
        if (door.isLocked) {
            // Draw lock indicator (small red cube)
            Vector3 lockPos = pos;
            lockPos.y = WALL_HEIGHT * 0.4f;
            DrawCube(lockPos, 0.15f, 0.15f, 0.15f, RED);
        }
    }
#endif
}

void GameMap::renderSwitches() const {
#if !defined(KAIKAI_HEADLESS)
    for (const auto& sw : switches) {
        Vector3 pos = sw.position;
        pos.y = 0.6f; // Slightly above floor on wall

        // Switch base
        Color swColor = sw.isPressed ? GREEN : YELLOW;
        if (sw.requiresTwoPlayers) {
            swColor = sw.isPressed ? SKYBLUE : ORANGE;
        }

        DrawCube(pos, 0.3f, 0.5f, 0.1f, swColor);

        // Indicator light
        Vector3 lightPos = pos;
        lightPos.y += 0.35f;
        Color lightColor = sw.isPressed ? GREEN : RED;
        DrawSphere(lightPos, 0.05f, lightColor);
    }
#endif
}

void GameMap::renderItems() const {
#if !defined(KAIKAI_HEADLESS)
    for (const auto& item : items) {
        if (item.isPickedUp) continue;

        Vector3 pos = item.position;
        // Items float and bob slightly
        float bobOffset = sinf(GetTime() * 3.0f + static_cast<float>(item.id)) * 0.1f;
        pos.y += bobOffset;

        // Rotate items for visual flair
        float rotation = GetTime() * 2.0f;

        switch (item.type) {
            case Item::KEY: {
                DrawCube(pos, 0.2f, 0.3f, 0.08f, GOLD);
                Vector3 keyEnd = pos;
                keyEnd.x += 0.2f;
                DrawCube(keyEnd, 0.1f, 0.1f, 0.08f, GOLD);
                break;
            }
            case Item::BATTERY: {
                DrawCube(pos, 0.15f, 0.3f, 0.15f, LIME);
                Vector3 terminal = pos;
                terminal.y += 0.18f;
                DrawCube(terminal, 0.08f, 0.06f, 0.08f, YELLOW);
                break;
            }
            case Item::NOTE: {
                DrawCube(pos, 0.3f, 0.02f, 0.4f, BEIGE);
                // Text lines on note (simple colored rectangles)
                Vector3 linePos = pos;
                linePos.y += 0.015f;
                for (int i = 0; i < 3; ++i) {
                    Vector3 lp = linePos;
                    lp.z += -0.1f + i * 0.1f;
                    DrawCube(lp, 0.2f, 0.005f, 0.02f, DARKGRAY);
                }
                break;
            }
            case Item::HEALTH_PACK: {
                DrawCube(pos, 0.25f, 0.25f, 0.25f, WHITE);
                // Red cross
                Vector3 cross1 = pos;
                cross1.y += 0.13f;
                DrawCube(cross1, 0.2f, 0.02f, 0.06f, RED);
                Vector3 cross2 = pos;
                cross2.y += 0.13f;
                DrawCube(cross2, 0.06f, 0.02f, 0.2f, RED);
                break;
            }
        }

    }
#endif
}
