#include "Game.h"
#include "raymath.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// ---------------------------------------------------------------------------
// Construction / Init
// ---------------------------------------------------------------------------

Game::Game(bool server) : isServer(server) {
}

void Game::init() {
    gameState = GameState::LOBBY;
    gameTime = 0.0f;
    lobbyTimer = 0.0f;
    survivorsWon = false;
    ghostPlayerId = 0;
    nextPlayerId = 1;
    switchPlayersPressing.clear();
    networkPlayers.clear();

    // Generate the procedural map
    gameMap.generate();

    // Create ghost entity (id=0 means unassigned until game starts)
    ghost = std::make_unique<Ghost>(0);

    printf("[Game] Initialised (isServer=%d).\n", isServer);
}

void Game::shutdown() {
    players.clear();
    ghost.reset();
    networkPlayers.clear();
}

// ---------------------------------------------------------------------------
// Main update dispatch
// ---------------------------------------------------------------------------

void Game::update(float deltaTime) {
    switch (gameState) {
        case GameState::LOBBY:
            updateLobby(deltaTime);
            break;
        case GameState::PLAYING:
            updatePlaying(deltaTime);
            break;
        case GameState::GAME_OVER:
            updateGameOver(deltaTime);
            break;
        case GameState::SPECTATING:
            updatePlaying(deltaTime);
            break;
    }
}

// ---------------------------------------------------------------------------
// Render dispatch
// ---------------------------------------------------------------------------

void Game::render() {
    switch (gameState) {
        case GameState::LOBBY:
            renderLobbyScreen();
            break;
        case GameState::PLAYING:
        case GameState::SPECTATING: {
            gameMap.render();

            // Render ghost visual indicator
            if (ghost) {
                const auto& gs = ghost->getState();
                Vector3 ghostPos = gs.position;
                ghostPos.y = 0.85f;
                DrawSphere(ghostPos, 0.4f, {200, 200, 255, 80});
            }

            renderHUD();
            break;
        }
        case GameState::GAME_OVER:
            gameMap.render();
            renderGameOverScreen();
            break;
    }
}

// ---------------------------------------------------------------------------
// Player management
// ---------------------------------------------------------------------------

uint32_t Game::addPlayer(const char* name) {
    if (players.size() >= 5) {
        fprintf(stderr, "[Game] Cannot add player - server full.\n");
        return 0;
    }

    uint32_t id = nextPlayerId++;
    auto player = std::make_unique<Player>(id);
    Vector3 sp = gameMap.getRandomSpawnPoint();
    player->getStateMut().position = sp;
    players[id] = std::move(player);

    if (localPlayerId == 0 && !isServer) {
        localPlayerId = id;
    }

    // Maintain the lightweight network mirror
    networkPlayers[id] = players[id]->getState();

    printf("[Game] Player added (id=%u, name=%s).\n", id, name);
    return id;
}

void Game::removePlayer(uint32_t id) {
    players.erase(id);
    networkPlayers.erase(id);

    if (ghostPlayerId == id) {
        ghostPlayerId = 0;
        if (gameState == GameState::PLAYING) {
            assignGhost();
        }
    }
    printf("[Game] Player removed (id=%u).\n", id);
}

Player* Game::getPlayer(uint32_t id) {
    auto it = players.find(id);
    return it != players.end() ? it->second.get() : nullptr;
}

Ghost* Game::getGhost() {
    return ghost.get();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void Game::startGame() {
    if (players.empty()) return;

    gameState = GameState::PLAYING;
    gameTime = 0.0f;
    survivorsWon = false;

    // Regenerate the map for a fresh round
    gameMap.generate();

    // Assign the ghost player
    assignGhost();

    // Position all players at spawn points
    for (auto& [id, player] : players) {
        Vector3 sp = gameMap.getRandomSpawnPoint();
        player->respawn(sp);
    }

    // Position ghost at ghost spawn
    if (ghost) {
        ghost->getStateMut().position = gameMap.getGhostSpawnPoint();
        ghost->possess();
    }

    // Sync network mirror
    syncNetworkPlayers();

    printf("[Game] Game started.\n");
}

void Game::endGame(bool survivorsWonFlag) {
    gameState = GameState::GAME_OVER;
    survivorsWon = survivorsWonFlag;
    printf("[Game] Game over - survivors %s.\n", survivorsWonFlag ? "won" : "lost");
}

void Game::assignGhost() {
    if (players.empty()) return;

    // Pick a random player to be the ghost
    auto it = players.begin();
    std::advance(it, rand() % players.size());
    uint32_t chosenId = it->first;

    ghostPlayerId = chosenId;

    // Create the ghost entity with the chosen player's ID
    ghost = std::make_unique<Ghost>(chosenId);
    ghost->possess();
    ghost->getStateMut().position = gameMap.getGhostSpawnPoint();

    // Mark the player entity as ghost
    Player* p = getPlayer(chosenId);
    if (p) {
        p->getStateMut().isGhost = true;
    }

    networkPlayers[chosenId].isGhost = true;

    printf("[Game] Player %u assigned as ghost.\n", chosenId);
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

GameState Game::getState() const { return gameState; }
GameMap& Game::getMap() { return gameMap; }

bool Game::isPlayerReady(uint32_t id) const {
    // All players are considered ready in this implementation
    // A full implementation would track ready state per player
    auto it = players.find(id);
    return it != players.end();
}

const std::unordered_map<uint32_t, std::unique_ptr<Player>>& Game::getPlayers() const {
    return players;
}

// ---------------------------------------------------------------------------
// Network sync
// ---------------------------------------------------------------------------

void Game::syncState() {
    syncNetworkPlayers();
}

void Game::applyState(const std::vector<PlayerState>& states) {
    for (const auto& s : states) {
        if (s.isGhost && ghost) {
            ghost->getStateMut() = s;
            continue;
        }

        Player* p = getPlayer(s.id);
        if (p) {
            p->getStateMut() = s;
        }

        networkPlayers[s.id] = s;
    }
}

void Game::syncNetworkPlayers() {
    for (const auto& [id, player] : players) {
        networkPlayers[id] = player->getState();
    }
    // Include ghost state
    if (ghost && ghostPlayerId != 0) {
        networkPlayers[ghostPlayerId] = ghost->getState();
    }
    // Remove entries for players that no longer exist
    for (auto it = networkPlayers.begin(); it != networkPlayers.end(); ) {
        if (players.find(it->first) == players.end() && it->first != ghostPlayerId) {
            it = networkPlayers.erase(it);
        } else {
            ++it;
        }
    }
}

// ---------------------------------------------------------------------------
// Networking-friendly accessors
// ---------------------------------------------------------------------------

PlayerState* Game::getPlayerState(uint32_t playerId) {
    syncNetworkPlayers();
    auto it = networkPlayers.find(playerId);
    return it != networkPlayers.end() ? &it->second : nullptr;
}

const PlayerState* Game::getPlayerState(uint32_t playerId) const {
    // const version — just look up from networkPlayers
    auto it = networkPlayers.find(playerId);
    if (it != networkPlayers.end()) return &it->second;
    // Fallback: look in players map
    auto pit = players.find(playerId);
    return pit != players.end() ? &pit->second->getState() : nullptr;
}

const std::unordered_map<uint32_t, PlayerState>& Game::getAllNetworkPlayers() const {
    // We need to update the mirror first, but this is const.
    // The server should call syncState() or syncNetworkPlayers() before this.
    return networkPlayers;
}

// ---------------------------------------------------------------------------
// Resource updates (server-authoritative)
// ---------------------------------------------------------------------------

void Game::updateSanity(uint32_t playerId, float sanity) {
    auto* p = getPlayer(playerId);
    if (p) p->getStateMut().sanity = std::clamp(sanity, 0.0f, 100.0f);
    auto it = networkPlayers.find(playerId);
    if (it != networkPlayers.end()) {
        it->second.sanity = std::clamp(sanity, 0.0f, 100.0f);
    }
}

void Game::updateBattery(uint32_t playerId, float battery) {
    auto* p = getPlayer(playerId);
    if (p) {
        p->getStateMut().battery = std::clamp(battery, 0.0f, 100.0f);
        if (p->getStateMut().battery <= 0.0f) p->getStateMut().flashlightOn = false;
    }
    auto it = networkPlayers.find(playerId);
    if (it != networkPlayers.end()) {
        it->second.battery = std::clamp(battery, 0.0f, 100.0f);
        if (it->second.battery <= 0.0f) it->second.flashlightOn = false;
    }
}

// ---------------------------------------------------------------------------
// Ghost catch
// ---------------------------------------------------------------------------

void Game::catchPlayer(uint32_t ghostId, uint32_t victimId) {
    auto* victim = getPlayer(victimId);
    if (!victim || victim->getState().isDead) return;

    victim->takeDamage(100.0f);

    // Update network mirror
    auto vit = networkPlayers.find(victimId);
    if (vit != networkPlayers.end()) {
        vit->second.isDead = true;
        vit->second.isSpectator = true;
    }

    printf("[Game] Ghost %u caught player %u.\n", ghostId, victimId);
    checkWinConditions();
}

// ---------------------------------------------------------------------------
// Item / door / switch wrappers for networking
// ---------------------------------------------------------------------------

bool Game::tryPickupItem(uint32_t playerId, uint32_t itemId) {
    Item* item = gameMap.getItem(itemId);
    if (!item || item->isPickedUp) return false;

    gameMap.pickUpItem(itemId, playerId);

    // Apply item effects
    auto* p = getPlayer(playerId);
    if (p) {
        switch (item->type) {
        case Item::KEY:
            printf("[Game] Player %u picked up a KEY.\n", playerId);
            break;
        case Item::BATTERY:
            p->getStateMut().battery = std::min(p->getState().battery + 50.0f, 100.0f);
            if (p->getState().battery > 0.0f && !p->getState().flashlightOn) {
                p->getStateMut().flashlightOn = true;
            }
            printf("[Game] Player %u picked up a BATTERY.\n", playerId);
            break;
        case Item::HEALTH_PACK:
            p->getStateMut().sanity = std::min(p->getState().sanity + 40.0f, 100.0f);
            printf("[Game] Player %u picked up a HEALTH PACK.\n", playerId);
            break;
        case Item::NOTE:
            printf("[Game] Player %u picked up a NOTE.\n", playerId);
            break;
        }
    }

    // Update network mirror
    auto it = networkPlayers.find(playerId);
    if (it != networkPlayers.end() && p) {
        it->second = p->getState();
    }

    return true;
}

void Game::toggleDoor(uint32_t doorId) {
    gameMap.toggleDoor(doorId);
}

void Game::setSwitch(uint32_t switchId, bool pressed) {
    if (pressed) {
        gameMap.pressSwitch(switchId, 0);
    } else {
        gameMap.releaseSwitch(switchId);
    }
}

Item* Game::getItem(uint32_t itemId) {
    return gameMap.getItem(itemId);
}

Door* Game::getDoor(uint32_t doorId) {
    return gameMap.getDoor(doorId);
}

Switch* Game::getSwitch(uint32_t switchId) {
    return gameMap.getSwitch(switchId);
}

// ---------------------------------------------------------------------------
// Interaction – comprehensive proximity-based interaction system
// ---------------------------------------------------------------------------

void Game::playerInteract(uint32_t playerId) {
    Player* player = getPlayer(playerId);
    if (!player) return;

    const auto& ps = player->getState();
    if (ps.isDead || ps.isGhost) return;

    bool interacted = false;

    // --- Priority 1: Nearby items (pick up) ---
    for (auto& item : gameMap.getItems()) {
        if (item.isPickedUp) continue;

        float dx = ps.position.x - item.position.x;
        float dz = ps.position.z - item.position.z;
        float dist = sqrtf(dx * dx + dz * dz);

        if (dist <= INTERACT_RANGE) {
            playerUseItem(playerId, item.id);
            interacted = true;
            break;
        }
    }
    if (interacted) return;

    // --- Priority 2: Nearby doors (toggle open/close) ---
    for (auto& door : gameMap.getDoors()) {
        float dx = ps.position.x - door.position.x;
        float dz = ps.position.z - door.position.z;
        float dist = sqrtf(dx * dx + dz * dz);

        if (dist <= INTERACT_RANGE) {
            if (door.isLocked) {
                bool hasKey = false;
                for (const auto& item : gameMap.getItems()) {
                    if (item.type == Item::KEY && item.isPickedUp) {
                        hasKey = true;
                        break;
                    }
                }
                if (hasKey) {
                    door.isLocked = false;
                    gameMap.toggleDoor(door.id);
                    printf("[Game] Player %u unlocked door %u.\n", playerId, door.id);
                }
            } else {
                gameMap.toggleDoor(door.id);
                printf("[Game] Player %u toggled door %u.\n", playerId, door.id);
            }
            interacted = true;
            break;
        }
    }
    if (interacted) return;

    // --- Priority 3: Nearby switches (press) ---
    for (auto& sw : gameMap.getSwitches()) {
        float dx = ps.position.x - sw.position.x;
        float dz = ps.position.z - sw.position.z;
        float dist = sqrtf(dx * dx + dz * dz);

        if (dist <= SWITCH_PRESS_RANGE) {
            gameMap.pressSwitch(sw.id, playerId);

            if (sw.requiresTwoPlayers) {
                auto& pressing = switchPlayersPressing[sw.id];
                bool alreadyPressing = false;
                for (uint32_t pid : pressing) {
                    if (pid == playerId) { alreadyPressing = true; break; }
                }
                if (!alreadyPressing) {
                    pressing.push_back(playerId);
                }
            }
            printf("[Game] Player %u pressed switch %u.\n", playerId, sw.id);
            break;
        }
    }
}

void Game::playerUseItem(uint32_t playerId, uint32_t itemId) {
    Player* player = getPlayer(playerId);
    if (!player) return;

    Item* item = gameMap.getItem(itemId);
    if (!item || item->isPickedUp) return;

    // Check distance
    float dx = player->getState().position.x - item->position.x;
    float dz = player->getState().position.z - item->position.z;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist > INTERACT_RANGE) return;

    gameMap.pickUpItem(itemId, playerId);

    auto& ps = player->getStateMut();

    switch (item->type) {
        case Item::KEY:
            printf("[Game] Player %u picked up a KEY.\n", playerId);
            break;
        case Item::BATTERY:
            ps.battery = std::min(ps.battery + 50.0f, 100.0f);
            if (ps.battery > 0.0f && !ps.flashlightOn) {
                ps.flashlightOn = true;
            }
            printf("[Game] Player %u picked up a BATTERY.\n", playerId);
            break;
        case Item::HEALTH_PACK:
            ps.sanity = std::min(ps.sanity + 40.0f, 100.0f);
            printf("[Game] Player %u picked up a HEALTH PACK.\n", playerId);
            break;
        case Item::NOTE:
            printf("[Game] Player %u picked up a NOTE.\n", playerId);
            break;
    }
}

// ---------------------------------------------------------------------------
// State updates
// ---------------------------------------------------------------------------

void Game::updateLobby(float deltaTime) {
    lobbyTimer += deltaTime;

    // Auto-start with enough players, or manual start with ENTER
    if (IsKeyPressed(KEY_ENTER) || (players.size() >= 2 && lobbyTimer > 3.0f)) {
        if (players.size() >= 1) {
            startGame();
        }
    }
}

void Game::updatePlaying(float deltaTime) {
    gameTime += deltaTime;

    // Collect survivor states for ghost targeting
    std::vector<PlayerState> survivorStates;
    for (const auto& [id, player] : players) {
        if (!player->getState().isGhost) {
            survivorStates.push_back(player->getState());
        }
    }

    // Update all players
    for (auto& [id, player] : players) {
        if (player->getState().isGhost) {
            // Ghost player controls the ghost entity
            if (ghost) {
                ghost->update(deltaTime, survivorStates);
                player->getStateMut().position = ghost->getState().position;
                player->getStateMut().rotation = ghost->getState().rotation;
            }
        } else {
            player->update(deltaTime, gameMap.getGridData());
        }
    }

    // Ghost proximity sanity drain
    handleGhostProximity(deltaTime);

    // Two-player switch logic
    handleTwoPlayerSwitches();

    // Release switches when E key is released
    if (IsKeyReleased(KEY_E)) {
        for (auto& [swId, pids] : switchPlayersPressing) {
            gameMap.releaseSwitch(swId);
        }
        switchPlayersPressing.clear();
    }

    // Ghost auto-catch when in range and cooldown is ready
    if (ghost && ghost->canCatch()) {
        for (auto& [id, player] : players) {
            if (player->getState().isGhost) continue;
            if (player->getState().isDead) continue;

            if (ghost->isInCatchRange(player->getState())) {
                ghost->catchPlayer(id);
                player->takeDamage(100.0f);
                printf("[Game] Ghost caught player %u.\n", id);
                break;
            }
        }
    }

    // Win conditions
    checkWinConditions();

    // Sync network mirror
    syncNetworkPlayers();
}

void Game::updateGameOver(float deltaTime) {
    (void)deltaTime;

    if (IsKeyPressed(KEY_ENTER)) {
        gameState = GameState::LOBBY;
        lobbyTimer = 0.0f;
        gameTime = 0.0f;

        for (auto& [id, player] : players) {
            player->respawn(gameMap.getRandomSpawnPoint());
            player->getStateMut().isGhost = false;
        }
        ghostPlayerId = 0;
    }
}

// ---------------------------------------------------------------------------
// Ghost proximity sanity drain
// ---------------------------------------------------------------------------

void Game::handleGhostProximity(float deltaTime) {
    if (!ghost) return;

    const auto& ghostPos = ghost->getState().position;

    for (auto& [id, player] : players) {
        if (player->getState().isGhost || player->getState().isDead) continue;

        float dx = player->getState().position.x - ghostPos.x;
        float dz = player->getState().position.z - ghostPos.z;
        float distSq = dx * dx + dz * dz;
        float rangeSq = GHOST_PROXIMITY_RANGE * GHOST_PROXIMITY_RANGE;

        if (distSq < rangeSq) {
            float dist = sqrtf(distSq);
            float intensity = 1.0f - (dist / GHOST_PROXIMITY_RANGE);
            intensity = intensity * intensity;
            float drain = GHOST_PROXIMITY_SANITY_DRAIN * intensity * deltaTime;
            player->takeDamage(drain);
        }
    }
}

// ---------------------------------------------------------------------------
// Two-player switch handling
// ---------------------------------------------------------------------------

void Game::handleTwoPlayerSwitches() {
    for (auto& [swId, pids] : switchPlayersPressing) {
        Switch* sw = gameMap.getSwitch(swId);
        if (!sw || !sw->requiresTwoPlayers) continue;

        if (pids.size() >= 2) {
            Door* d = gameMap.getDoor(sw->linkedDoorId);
            if (d && d->isLocked) {
                d->isLocked = false;
                gameMap.openDoor(d->id);
                printf("[Game] Two-player switch %u unlocked door %u.\n", swId, d->id);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Win conditions
// ---------------------------------------------------------------------------

void Game::checkWinConditions() {
    if (gameState != GameState::PLAYING) return;

    int aliveCount = 0;
    for (const auto& [id, player] : players) {
        if (!player->getState().isGhost && !player->getState().isDead) {
            aliveCount++;
        }
    }

    if (aliveCount == 0 && !players.empty()) {
        endGame(false);
        return;
    }

    if (gameTime >= GAME_TIME_LIMIT) {
        endGame(true);
    }
}

// ---------------------------------------------------------------------------
// HUD Rendering
// ---------------------------------------------------------------------------

void Game::renderHUD() const {
    const PlayerState* localState = nullptr;
    for (const auto& [id, player] : players) {
        if (id == localPlayerId || (!isServer && localPlayerId == 0)) {
            localState = &player->getState();
            break;
        }
    }

    if (!localState && !players.empty()) {
        localState = &players.begin()->second->getState();
    }

    if (!localState) return;

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // Stamina bar
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = screenHeight - 100;

    DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
    float staminaRatio = localState->stamina / 100.0f;
    Color staminaColor = staminaRatio > 0.3f ? GREEN : (staminaRatio > 0.15f ? YELLOW : RED);
    DrawRectangle(barX, barY, static_cast<int>(barWidth * staminaRatio), barHeight, staminaColor);
    DrawRectangleLines(barX, barY, barWidth, barHeight, WHITE);
    DrawText("STAMINA", barX, barY - 20, 16, WHITE);

    // Sanity bar
    barY += 40;
    DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
    float sanityRatio = localState->sanity / 100.0f;
    Color sanityColor = sanityRatio > 0.5f ? BLUE : (sanityRatio > 0.25f ? PURPLE : RED);
    DrawRectangle(barX, barY, static_cast<int>(barWidth * sanityRatio), barHeight, sanityColor);
    DrawRectangleLines(barX, barY, barWidth, barHeight, WHITE);
    DrawText("SANITY", barX, barY - 20, 16, WHITE);

    // Battery bar
    barY += 40;
    DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
    float batteryRatio = localState->battery / 100.0f;
    Color batteryColor = batteryRatio > 0.3f ? YELLOW : (batteryRatio > 0.15f ? ORANGE : RED);
    DrawRectangle(barX, barY, static_cast<int>(barWidth * batteryRatio), barHeight, batteryColor);
    DrawRectangleLines(barX, barY, barWidth, barHeight, WHITE);
    DrawText("BATTERY", barX, barY - 20, 16, WHITE);

    // Flashlight indicator
    const char* flashlightText = localState->flashlightOn ? "FLASHLIGHT: ON" : "FLASHLIGHT: OFF";
    Color flColor = localState->flashlightOn ? YELLOW : GRAY;
    DrawText(flashlightText, screenWidth - 200, screenHeight - 40, 18, flColor);

    // Game timer
    int minutes = static_cast<int>(gameTime) / 60;
    int seconds = static_cast<int>(gameTime) % 60;
    DrawText(TextFormat("%02d:%02d", minutes, seconds), screenWidth / 2 - 40, 20, 32, WHITE);

    // Player role
    if (localState->isGhost) {
        DrawText("YOU ARE THE GHOST", screenWidth / 2 - 120, 60, 24, RED);
        if (ghost) {
            const char* catchText = ghost->canCatch() ? "[E] CATCH" : "COOLDOWN...";
            DrawText(catchText, screenWidth / 2 - 50, 90, 20,
                     ghost->canCatch() ? RED : GRAY);
        }
    } else if (localState->isDead) {
        DrawText("YOU ARE DEAD (SPECTATING)", screenWidth / 2 - 160, 60, 24, GRAY);
    } else {
        DrawText("SURVIVOR", screenWidth / 2 - 50, 60, 20, GREEN);
    }

    // Interaction prompt
    if (!localState->isDead && !localState->isGhost) {
        for (const auto& item : gameMap.getItems()) {
            if (item.isPickedUp) continue;
            float dx = localState->position.x - item.position.x;
            float dz = localState->position.z - item.position.z;
            if (sqrtf(dx * dx + dz * dz) <= INTERACT_RANGE) {
                const char* itemName = "";
                switch (item.type) {
                    case Item::KEY:         itemName = "KEY"; break;
                    case Item::BATTERY:     itemName = "BATTERY"; break;
                    case Item::NOTE:        itemName = "NOTE"; break;
                    case Item::HEALTH_PACK: itemName = "HEALTH PACK"; break;
                }
                DrawText(TextFormat("[E] Pick up %s", itemName),
                         screenWidth / 2 - 80, screenHeight / 2 + 60, 20, YELLOW);
                break;
            }
        }

        for (const auto& door : gameMap.getDoors()) {
            if (door.isOpen) continue;
            float dx = localState->position.x - door.position.x;
            float dz = localState->position.z - door.position.z;
            if (sqrtf(dx * dx + dz * dz) <= INTERACT_RANGE) {
                if (door.isLocked) {
                    DrawText("[E] Locked (need key)", screenWidth / 2 - 90, screenHeight / 2 + 60, 20, RED);
                } else {
                    DrawText("[E] Open door", screenWidth / 2 - 60, screenHeight / 2 + 60, 20, YELLOW);
                }
                break;
            }
        }
    }

    // Sanity visual effects
    if (localState->sanity < 30.0f && !localState->isGhost) {
        float intensity = 1.0f - (localState->sanity / 30.0f);
        int vignetteAlpha = static_cast<int>(intensity * 120.0f);
        Color vignetteColor = {150, 0, 0, static_cast<unsigned char>(vignetteAlpha)};

        DrawRectangle(0, 0, screenWidth, 40, vignetteColor);
        DrawRectangle(0, screenHeight - 40, screenWidth, 40, vignetteColor);
        DrawRectangle(0, 0, 40, screenHeight, vignetteColor);
        DrawRectangle(screenWidth - 40, 0, 40, screenHeight, vignetteColor);
    }
}

// ---------------------------------------------------------------------------
// Lobby screen
// ---------------------------------------------------------------------------

void Game::renderLobbyScreen() const {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);
    DrawText("K A I K A I", screenWidth / 2 - 150, 100, 60, RED);
    DrawText("3D Multiplayer Horror", screenWidth / 2 - 150, 170, 24, DARKGRAY);

    int y = 250;
    DrawText("PLAYERS IN LOBBY:", screenWidth / 2 - 100, y, 20, WHITE);
    y += 30;
    for (const auto& [id, player] : players) {
        const char* roleText = player->getState().isGhost ? " [GHOST]" : "";
        DrawText(TextFormat("Player %u%s", id, roleText),
                 screenWidth / 2 - 80, y, 18,
                 player->getState().isGhost ? RED : GREEN);
        y += 25;
    }

    DrawText("Press ENTER to start the game", screenWidth / 2 - 160, screenHeight - 100, 20, YELLOW);
    DrawText("Minimum 2 players recommended (1 ghost + 1 survivor)",
             screenWidth / 2 - 250, screenHeight - 70, 16, GRAY);
}

// ---------------------------------------------------------------------------
// Game over screen
// ---------------------------------------------------------------------------

void Game::renderGameOverScreen() const {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    DrawRectangle(0, 0, screenWidth, screenHeight, {0, 0, 0, 180});

    if (survivorsWon) {
        DrawText("SURVIVORS WIN!", screenWidth / 2 - 180, screenHeight / 2 - 40, 48, GREEN);
        DrawText("You survived the haunting...", screenWidth / 2 - 180, screenHeight / 2 + 30, 24, WHITE);
    } else {
        DrawText("GHOST WINS!", screenWidth / 2 - 140, screenHeight / 2 - 40, 48, RED);
        DrawText("The ghost consumed your sanity...", screenWidth / 2 - 190, screenHeight / 2 + 30, 24, GRAY);
    }

    DrawText("Press ENTER to return to lobby", screenWidth / 2 - 160, screenHeight / 2 + 80, 20, YELLOW);
}
