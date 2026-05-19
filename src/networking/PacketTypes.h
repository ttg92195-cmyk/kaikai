#pragma once
#if defined(KAIKAI_HEADLESS)
#include "../utils/HeadlessCompat.h"
#else
#include "raylib.h"
#endif
#include "../utils/Constants.h"
#include <cstdint>
#include <cstring>

// Network constants (unique to networking, not duplicated from Constants.h)
using Kaikai::SERVER_PORT;
using Kaikai::MAX_PLAYERS;
using Kaikai::TICK_RATE;
using Kaikai::NETWORK_UPDATE_RATE;
using Kaikai::VOICE_CHAT_MAX_DISTANCE;

constexpr float    MAX_MOVE_SPEED     = 8.0f;  // units per second (anti-cheat)
constexpr float    MAX_RUN_SPEED      = 14.0f;
constexpr float    STATE_BROADCAST_INTERVAL = 1.0f / NETWORK_UPDATE_RATE;
constexpr uint8_t  CHANNEL_RELIABLE   = 0;
constexpr uint8_t  CHANNEL_UNRELIABLE = 1;
constexpr uint8_t  CHANNEL_VOICE      = 2;

enum class PacketType : uint8_t {
    PLAYER_JOIN, PLAYER_LEAVE, PLAYER_MOVE, PLAYER_ACTION,
    GAME_STATE, CHAT_MESSAGE, VOICE_DATA, ITEM_PICKUP,
    DOOR_TOGGLE, SWITCH_TOGGLE, GHOST_CATCH, SANITY_UPDATE,
    BATTERY_UPDATE, JUMPSCARE_TRIGGER, ITEM_SPAWN
};

// Shared player state — canonical definition used throughout the engine.
// Both the game simulation and the networking layer reference this struct.
struct PlayerState {
    uint32_t id           = 0;
    Vector3  position     = { 0.0f, 0.0f, 0.0f };
    float    rotation     = 0.0f;
    float    stamina      = 100.0f;
    float    sanity       = 100.0f;
    float    battery      = 100.0f;
    bool     isGhost      = false;
    bool     isDead       = false;
    bool     isSpectator  = false;
    bool     flashlightOn = false;
    bool     isRunning    = false;   // used by game simulation
    float    headBob      = 0.0f;    // client-side rendering
    float    footstepTimer = 0.0f;   // client-side audio
};

// -------------------------------------------------------------------
// Base header present in every packet
// -------------------------------------------------------------------
struct PacketHeader {
    PacketType type;
    uint32_t   senderId;
    uint32_t   timestamp;  // milliseconds since server start
};

// -------------------------------------------------------------------
// Connection lifecycle
// -------------------------------------------------------------------
struct PlayerJoinPacket {
    PacketHeader header;
    char         playerName[32];
};

struct PlayerLeavePacket {
    PacketHeader header;
};

// -------------------------------------------------------------------
// Movement
// -------------------------------------------------------------------
struct PlayerMovePacket {
    PacketHeader header;
    Vector3     position;
    float       rotation;
    bool        isRunning;
    bool        flashlightOn;
};

// -------------------------------------------------------------------
// Player actions / interactions
// -------------------------------------------------------------------
struct PlayerActionPacket {
    PacketHeader header;
    enum Action : uint8_t {
        INTERACT, TOGGLE_FLASHLIGHT, PICK_UP_ITEM,
        PRESS_SWITCH, RELEASE_SWITCH, USE_ITEM
    } action;
    uint32_t targetId;  // item / door / switch id
};

// -------------------------------------------------------------------
// Full game-state snapshot (server -> clients)
// -------------------------------------------------------------------
struct GameStatePacket {
    PacketHeader header;
    uint8_t      playerCount;
    PlayerState  players[MAX_PLAYERS];
    uint8_t      gameState;  // 0 = lobby, 1 = playing, 2 = gameover
};

// -------------------------------------------------------------------
// Chat
// -------------------------------------------------------------------
struct ChatMessagePacket {
    PacketHeader header;
    char         message[256];
};

// -------------------------------------------------------------------
// Proximity voice
// -------------------------------------------------------------------
struct VoiceDataPacket {
    PacketHeader header;
    uint8_t      data[1024];
    uint32_t     dataLength;
    float        volume;  // for proximity calculation
};

// -------------------------------------------------------------------
// Items
// -------------------------------------------------------------------
struct ItemPickupPacket {
    PacketHeader header;
    uint32_t     itemId;
    uint8_t      itemType;  // 0 = key, 1 = battery, 2 = note, 3 = health
};

struct ItemSpawnPacket {
    PacketHeader header;
    uint8_t      itemType;
    Vector3      position;
    uint32_t     itemId;
};

// -------------------------------------------------------------------
// World interactions
// -------------------------------------------------------------------
struct DoorTogglePacket {
    PacketHeader header;
    uint32_t     doorId;
    bool         open;
};

struct SwitchTogglePacket {
    PacketHeader header;
    uint32_t     switchId;
    bool         pressed;
};

// -------------------------------------------------------------------
// Ghost mechanics
// -------------------------------------------------------------------
struct GhostCatchPacket {
    PacketHeader header;
    uint32_t     victimId;
};

// -------------------------------------------------------------------
// Per-player resource updates
// -------------------------------------------------------------------
struct SanityUpdatePacket {
    PacketHeader header;
    float        sanity;
};

struct BatteryUpdatePacket {
    PacketHeader header;
    float        battery;
};

// -------------------------------------------------------------------
// Jump-scare event
// -------------------------------------------------------------------
struct JumpscarePacket {
    PacketHeader header;
    uint32_t     targetPlayerId;
    float        duration;
};
