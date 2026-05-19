#pragma once
#include "raylib.h"
#include "../game/Game.h"
#include "Flashlight.h"
#include "FogSystem.h"
#include <cstdint>

class Renderer {
public:
    Renderer();
    ~Renderer() = default;

    void init(int screenWidth, int screenHeight);
    void beginFrame();
    void endFrame();

    void renderGame(const Game& game, const PlayerState& localPlayer);
    void renderLobby(const Game& game);
    void renderGameOver(bool survivorsWon);
    void renderHUD(const PlayerState& player);
    void renderSanityEffects(float sanity);
    void renderCrosshair();
    void renderMinimap(const Game& game, const PlayerState& localPlayer);
    void renderInteractionPrompt(const char* text);

    // Set map data for rendering (called by the Game or level loader)
    void setMapData(const uint8_t* data, int width, int height);

    void shutdown();

private:
    Shader flashlightShader;
    Shader fogShader;
    Shader sanityShader;
    RenderTexture2D sceneTexture;
    int screenWidth = 1280;
    int screenHeight = 720;
    bool shadersLoaded = false;

    // Sub-systems
    Flashlight flashlight;
    FogSystem fogSystem;

    // Map data reference (not owned)
    const uint8_t* mapData = nullptr;
    int mapWidth = 0;
    int mapHeight = 0;

    // Explored tiles for minimap
    bool* exploredTiles = nullptr;

    // Cached state for endFrame compositing
    PlayerState cachedPlayerState = {};
    const Game* cachedGame = nullptr;
    float cachedSanity = 100.0f;
    float animationTime = 0.0f;

    // ---- Shader uniform locations ----
    // Flashlight shader
    int flPlayerPosLoc = -1;
    int flPlayerDirLoc = -1;
    int flFlickerLoc   = -1;
    int flAmbientLoc   = -1;
    int flFogDensityLoc = -1;
    int flFogColorLoc   = -1;
    int flTimeLoc       = -1;

    // Sanity shader
    int sanSanityLoc     = -1;
    int sanTimeLoc       = -1;
    int sanResolutionLoc = -1;

    // ---- Player model colors ----
    static constexpr int MAX_PLAYER_COLORS = 8;
    static const Color playerColors[MAX_PLAYER_COLORS];

    // ---- Private rendering helpers ----
    void loadShaders();
    void unloadShaders();

    Camera3D buildCamera(const PlayerState& player) const;
    void updateFlashlightUniforms(const PlayerState& player);
    void updateSanityUniforms(float sanity);

    void renderMap() const;
    void renderFloorAndCeiling() const;
    void renderWalls() const;
    void renderDoors() const;
    void renderSwitches() const;
    void renderItems() const;

    void renderPlayers(const Game& game, uint32_t localPlayerId) const;
    void renderPlayerModel(Vector3 pos, float rotation, Color bodyColor, bool isGhost, float time) const;

    void updateExplored(const PlayerState& localPlayer);
};
