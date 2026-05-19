#pragma once
#include "Player.h"
#include "Ghost.h"
#include "Map.h"
#include <vector>
#include <memory>
#include <unordered_map>

enum class GameState {
    LOBBY,
    PLAYING,
    GAME_OVER,
    SPECTATING
};

class Game {
public:
    Game(bool isServer = false);
    ~Game() = default;

    void init();
    void update(float deltaTime);
    void render();
    void shutdown();

    // Player management
    uint32_t addPlayer(const char* name = "Player");
    void     removePlayer(uint32_t id);
    Player*  getPlayer(uint32_t id);
    Ghost*   getGhost();

    // Game flow
    void startGame();
    void endGame(bool survivorsWon);
    void assignGhost();

    // Getters
    GameState getState() const;
    GameMap&  getMap();
    const std::unordered_map<uint32_t, std::unique_ptr<Player>>& getPlayers() const;

    // Network sync
    void syncState();
    void applyState(const std::vector<PlayerState>& states);

    // Interaction
    void playerInteract(uint32_t playerId);
    void playerUseItem(uint32_t playerId, uint32_t itemId);

    // ------------------------------------------------------------------
    // Networking-friendly accessors
    // Thin wrappers the Server / Client use to read / write the
    // authoritative game state without needing the full Player objects.
    // ------------------------------------------------------------------
    PlayerState* getPlayerState(uint32_t playerId);
    const PlayerState* getPlayerState(uint32_t playerId) const;
    const std::unordered_map<uint32_t, PlayerState>& getAllNetworkPlayers() const;

    // Resource updates (server-authoritative)
    void updateSanity(uint32_t playerId, float sanity);
    void updateBattery(uint32_t playerId, float battery);

    // Ghost catch
    void catchPlayer(uint32_t ghostId, uint32_t victimId);

    // Item / door / switch wrappers for networking
    bool  tryPickupItem(uint32_t playerId, uint32_t itemId);
    void  toggleDoor(uint32_t doorId);
    void  setSwitch(uint32_t switchId, bool pressed);

    // Direct access to map objects
    Item*   getItem(uint32_t itemId);
    Door*   getDoor(uint32_t doorId);
    Switch* getSwitch(uint32_t switchId);

private:
    bool isServer;
    GameState gameState = GameState::LOBBY;
    GameMap gameMap;
    std::unordered_map<uint32_t, std::unique_ptr<Player>> players;
    std::unique_ptr<Ghost> ghost;
    uint32_t nextPlayerId = 1;
    uint32_t ghostPlayerId = 0;
    float gameTime = 0.0f;
    float lobbyTimer = 0.0f;
    bool survivorsWon = false;
    uint32_t localPlayerId = 0;

    // Lightweight network player states — kept in sync with Player objects
    // so the networking layer can work with plain data.
    std::unordered_map<uint32_t, PlayerState> networkPlayers;
    void syncNetworkPlayers();

    // Item interaction distance
    static constexpr float INTERACT_RANGE = 2.5f;
    static constexpr float SWITCH_PRESS_RANGE = 2.0f;

    // Game time limit (seconds)
    static constexpr float GAME_TIME_LIMIT = 300.0f;

    // Sanity drain from ghost proximity
    static constexpr float GHOST_PROXIMITY_SANITY_DRAIN = 15.0f;
    static constexpr float GHOST_PROXIMITY_RANGE = 8.0f;

    // Two-player switch tracking
    std::unordered_map<uint32_t, std::vector<uint32_t>> switchPlayersPressing;

    void updateLobby(float deltaTime);
    void updatePlaying(float deltaTime);
    void updateGameOver(float deltaTime);
    void handleGhostProximity(float deltaTime);
    void handleTwoPlayerSwitches();
    void checkWinConditions();
    void renderHUD() const;
    void renderLobbyScreen() const;
    void renderGameOverScreen() const;
};
