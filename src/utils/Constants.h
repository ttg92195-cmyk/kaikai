#pragma once

#include <cstdint>

namespace Kaikai {

// ---------------------------------------------------------------------------
// Map
// ---------------------------------------------------------------------------
constexpr int32_t MAP_WIDTH   = 50;
constexpr int32_t MAP_HEIGHT  = 50;
constexpr float   TILE_SIZE   = 2.0f;

// ---------------------------------------------------------------------------
// Players
// ---------------------------------------------------------------------------
constexpr int32_t MAX_PLAYERS = 5;  // 4 survivors + 1 ghost

// ---------------------------------------------------------------------------
// Default stat values
// ---------------------------------------------------------------------------
constexpr float DEFAULT_STAMINA  = 100.0f;
constexpr float DEFAULT_SANITY   = 100.0f;
constexpr float DEFAULT_BATTERY  = 100.0f;

// ---------------------------------------------------------------------------
// Stamina
// ---------------------------------------------------------------------------
constexpr float STAMINA_DRAIN_RATE = 10.0f;  // per second while running
constexpr float STAMINA_REGEN_RATE = 5.0f;   // per second while walking / still

// ---------------------------------------------------------------------------
// Sanity
// ---------------------------------------------------------------------------
constexpr float SANITY_DRAIN_RATE = 2.0f;  // per second while in the dark
constexpr float SANITY_REGEN_RATE = 1.0f;  // per second in light / safe zone

// ---------------------------------------------------------------------------
// Flashlight / Battery
// ---------------------------------------------------------------------------
constexpr float BATTERY_DRAIN_RATE           = 3.0f;   // per second while on
constexpr float FLASHLIGHT_FLICKER_THRESHOLD = 20.0f;  // battery % below which the beam flickers

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------
constexpr float GHOST_SPEED       = 4.5f;
constexpr float PLAYER_WALK_SPEED = 3.0f;
constexpr float PLAYER_RUN_SPEED  = 5.5f;

// ---------------------------------------------------------------------------
// Audio / Voice
// ---------------------------------------------------------------------------
constexpr float SOUND_DETECTION_RADIUS    = 15.0f;
constexpr float VOICE_CHAT_MAX_DISTANCE   = 30.0f;

// ---------------------------------------------------------------------------
// Jumpscare
// ---------------------------------------------------------------------------
constexpr float JUMPSCARE_DURATION = 0.5f;  // seconds

// ---------------------------------------------------------------------------
// Networking
// ---------------------------------------------------------------------------
constexpr uint16_t SERVER_PORT         = 7777;
constexpr int32_t  TICK_RATE           = 60;   // simulation ticks per second
constexpr int32_t  NETWORK_UPDATE_RATE = 20;   // packets sent per second

// ---------------------------------------------------------------------------
// Derived constants
// ---------------------------------------------------------------------------
constexpr float MAP_WORLD_WIDTH  = MAP_WIDTH  * TILE_SIZE;
constexpr float MAP_WORLD_HEIGHT = MAP_HEIGHT * TILE_SIZE;

// ---------------------------------------------------------------------------
// Item spawning
// ---------------------------------------------------------------------------
constexpr int32_t MAX_ITEMS_ON_MAP       = 20;
constexpr float   ITEM_SPAWN_INTERVAL    = 30.0f;   // seconds between spawn waves
constexpr float   ITEM_INTERACT_DISTANCE = 2.5f;

// ---------------------------------------------------------------------------
// Ghost mechanics
// ---------------------------------------------------------------------------
constexpr float GHOST_CATCH_RADIUS       = 1.8f;
constexpr float GHOST_VISIBILITY_RADIUS  = 12.0f;  // survivors can see ghost this close
constexpr float GHOST_INVISIBILITY_DELAY = 5.0f;   // seconds before ghost can go invisible

// ---------------------------------------------------------------------------
// Door / Switch
// ---------------------------------------------------------------------------
constexpr float DOOR_INTERACT_DISTANCE   = 2.5f;
constexpr float SWITCH_INTERACT_DISTANCE = 2.5f;

// ---------------------------------------------------------------------------
// Fog
// ---------------------------------------------------------------------------
constexpr float FOG_NEAR_PLANE = 1.0f;
constexpr float FOG_FAR_PLANE  = 25.0f;
constexpr float FOG_DENSITY    = 0.04f;

// ---------------------------------------------------------------------------
// Spectator
// ---------------------------------------------------------------------------
constexpr float SPECTATOR_MOVE_SPEED   = 8.0f;
constexpr float SPECTATOR_ASCEND_SPEED = 4.0f;

}  // namespace Kaikai
