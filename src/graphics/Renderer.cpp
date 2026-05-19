#include "Renderer.h"
#include "ShaderCode.h"
#include "../utils/Constants.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

using namespace Kaikai;

// ============================================================================
// Static member initialization
// ============================================================================
const Color Renderer::playerColors[MAX_PLAYER_COLORS] = {
    {50, 100, 220, 255},    // Blue
    {50, 200, 80, 255},     // Green
    {230, 140, 30, 255},    // Orange
    {30, 200, 210, 255},    // Cyan
    {220, 70, 150, 255},    // Pink
    {220, 210, 40, 255},    // Yellow
    {120, 210, 50, 255},    // Lime
    {40, 180, 170, 255}     // Teal
};

// ============================================================================
// Construction / Destruction
// ============================================================================
Renderer::Renderer()
    : screenWidth(1280)
    , screenHeight(720)
    , shadersLoaded(false)
    , mapData(nullptr)
    , mapWidth(0)
    , mapHeight(0)
    , exploredTiles(nullptr)
    , cachedGame(nullptr)
    , cachedSanity(100.0f)
    , animationTime(0.0f)
{
    flashlightShader = {0};
    fogShader = {0};
    sanityShader = {0};
    sceneTexture = {0};
}

// ============================================================================
// Initialization
// ============================================================================
void Renderer::init(int width, int height)
{
    screenWidth = width;
    screenHeight = height;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Kaikai - Multiplayer Horror");
    SetTargetFPS(60);

    // Create the render texture for multi-pass rendering
    sceneTexture = LoadRenderTexture(screenWidth, screenHeight);

    // Load custom shaders
    loadShaders();

    // Allocate explored tiles array
    exploredTiles = new bool[GRID_WIDTH * GRID_HEIGHT];
    memset(exploredTiles, 0, sizeof(bool) * GRID_WIDTH * GRID_HEIGHT);
}

// ============================================================================
// Shutdown
// ============================================================================
void Renderer::shutdown()
{
    unloadShaders();
    UnloadRenderTexture(sceneTexture);

    delete[] exploredTiles;
    exploredTiles = nullptr;

    CloseWindow();
}

// ============================================================================
// Shader loading from embedded GLSL code
// ============================================================================
void Renderer::loadShaders()
{
    // --- Flashlight shader (per-object 3D lighting + fog) ---
    flashlightShader = LoadShaderFromMemory(
        ShaderCode::getFlashlightVertexShader(),
        ShaderCode::getFlashlightFragmentShader()
    );

    if (flashlightShader.id != 0) {
        flPlayerPosLoc  = GetShaderLocation(flashlightShader, "playerPos");
        flPlayerDirLoc  = GetShaderLocation(flashlightShader, "playerDir");
        flFlickerLoc    = GetShaderLocation(flashlightShader, "flickerIntensity");
        flAmbientLoc    = GetShaderLocation(flashlightShader, "ambientStrength");
        flFogDensityLoc = GetShaderLocation(flashlightShader, "fogDensity");
        flFogColorLoc   = GetShaderLocation(flashlightShader, "fogColor");
        flTimeLoc       = GetShaderLocation(flashlightShader, "time");

        // Set initial ambient (very dark scene without flashlight)
        float ambient = 0.04f;
        SetShaderValue(flashlightShader, flAmbientLoc, &ambient, SHADER_UNIFORM_FLOAT);

        // Set initial fog parameters
        float fogD = fogSystem.getFogDensity();
        SetShaderValue(flashlightShader, flFogDensityLoc, &fogD, SHADER_UNIFORM_FLOAT);

        float fogCol[3] = {20.0f / 255.0f, 15.0f / 255.0f, 25.0f / 255.0f};
        SetShaderValue(flashlightShader, flFogColorLoc, fogCol, SHADER_UNIFORM_VEC3);

        float t = 0.0f;
        SetShaderValue(flashlightShader, flTimeLoc, &t, SHADER_UNIFORM_FLOAT);
    }

    // --- Fog post-process shader ---
    fogShader = LoadShaderFromMemory(
        ShaderCode::getFogVertexShader(),
        ShaderCode::getFogFragmentShader()
    );
    // Fog shader is available for supplemental use but the primary fog
    // is handled by the flashlight fragment shader for per-object integration.

    // --- Sanity post-process shader ---
    sanityShader = LoadShaderFromMemory(
        ShaderCode::getSanityVertexShader(),
        ShaderCode::getSanityFragmentShader()
    );

    if (sanityShader.id != 0) {
        sanSanityLoc     = GetShaderLocation(sanityShader, "sanity");
        sanTimeLoc       = GetShaderLocation(sanityShader, "time");
        sanResolutionLoc = GetShaderLocation(sanityShader, "resolution");

        float initSanity = 100.0f;
        SetShaderValue(sanityShader, sanSanityLoc, &initSanity, SHADER_UNIFORM_FLOAT);

        float initTime = 0.0f;
        SetShaderValue(sanityShader, sanTimeLoc, &initTime, SHADER_UNIFORM_FLOAT);

        float res[2] = {(float)screenWidth, (float)screenHeight};
        SetShaderValue(sanityShader, sanResolutionLoc, res, SHADER_UNIFORM_VEC2);
    }

    shadersLoaded = (flashlightShader.id != 0 && sanityShader.id != 0);
}

// ============================================================================
// Shader unloading
// ============================================================================
void Renderer::unloadShaders()
{
    if (flashlightShader.id != 0) UnloadShader(flashlightShader);
    if (fogShader.id != 0)        UnloadShader(fogShader);
    if (sanityShader.id != 0)     UnloadShader(sanityShader);
    shadersLoaded = false;
}

// ============================================================================
// Set map data pointer (not owned by Renderer)
// ============================================================================
void Renderer::setMapData(const uint8_t* data, int width, int height)
{
    mapData = data;
    mapWidth = width;
    mapHeight = height;

    // Reallocate explored tiles for new map size
    delete[] exploredTiles;
    exploredTiles = new bool[width * height];
    memset(exploredTiles, 0, sizeof(bool) * width * height);
}

// ============================================================================
// Begin a new frame
// ============================================================================
void Renderer::beginFrame()
{
    float dt = GetFrameTime();
    animationTime += dt;

    BeginDrawing();
    ClearBackground({5, 3, 10, 255});
}

// ============================================================================
// End the current frame – compositing and presentation
// ============================================================================
void Renderer::endFrame()
{
    // If a 3D game scene was rendered, composite it with post-processing
    if (cachedGame != nullptr && cachedPlayerState.id != 0) {
        // Draw the scene render texture to screen, applying sanity post-processing
        bool useSanityShader = shadersLoaded && (cachedSanity < 98.0f);

        if (useSanityShader) {
            updateSanityUniforms(cachedSanity);
            BeginShaderMode(sanityShader);
        }

        // Flip Y because OpenGL render textures are bottom-up
        DrawTextureRec(
            sceneTexture.texture,
            {0, 0, (float)sceneTexture.texture.width, (float)-sceneTexture.texture.height},
            {0, 0},
            WHITE
        );

        if (useSanityShader) {
            EndShaderMode();
        }

        // Draw HUD on top (not affected by sanity shader)
        renderHUD(cachedPlayerState);
        renderCrosshair();
        renderMinimap(*cachedGame, cachedPlayerState);
    }

    EndDrawing();

    // Reset cached state
    cachedGame = nullptr;
}

// ============================================================================
// Build a Camera3D from a PlayerState
// ============================================================================
Camera3D Renderer::buildCamera(const PlayerState& player) const
{
    Camera3D cam = {0};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 70.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    // Eye position: player position + eye height
    float eyeHeight = PLAYER_HEIGHT * 0.9f;
    cam.position = {
        player.position.x,
        player.position.y + eyeHeight,
        player.position.z
    };

    // Forward direction from rotation (yaw)
    float yaw = player.rotation;
    Vector3 forward = {
        sinf(yaw),
        0.0f,
        cosf(yaw)
    };

    cam.target = {
        cam.position.x + forward.x,
        cam.position.y,
        cam.position.z + forward.z
    };

    return cam;
}

// ============================================================================
// Update flashlight shader uniforms
// ============================================================================
void Renderer::updateFlashlightUniforms(const PlayerState& player)
{
    if (flashlightShader.id == 0) return;

    Camera3D cam = buildCamera(player);

    // Player eye position
    float eyeHeight = PLAYER_HEIGHT * 0.9f;
    float pos[3] = {player.position.x, player.position.y + eyeHeight, player.position.z};
    SetShaderValue(flashlightShader, flPlayerPosLoc, pos, SHADER_UNIFORM_VEC3);

    // Player forward direction
    float yaw = player.rotation;
    float dir[3] = {sinf(yaw), 0.0f, cosf(yaw)};
    SetShaderValue(flashlightShader, flPlayerDirLoc, dir, SHADER_UNIFORM_VEC3);

    // Flicker intensity (from flashlight system)
    float flicker = player.flashlightOn ? flashlight.getIntensity() : 0.0f;
    SetShaderValue(flashlightShader, flFlickerLoc, &flicker, SHADER_UNIFORM_FLOAT);

    // Time for fog animation
    SetShaderValue(flashlightShader, flTimeLoc, &animationTime, SHADER_UNIFORM_FLOAT);

    // Fog density (animated slightly)
    float fogD = fogSystem.getFogDensity() * (1.0f + 0.08f * sinf(animationTime * 0.3f));
    SetShaderValue(flashlightShader, flFogDensityLoc, &fogD, SHADER_UNIFORM_FLOAT);
}

// ============================================================================
// Update sanity shader uniforms
// ============================================================================
void Renderer::updateSanityUniforms(float sanity)
{
    if (sanityShader.id == 0) return;

    float s = sanity;
    SetShaderValue(sanityShader, sanSanityLoc, &s, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sanityShader, sanTimeLoc, &animationTime, SHADER_UNIFORM_FLOAT);

    float res[2] = {(float)screenWidth, (float)screenHeight};
    SetShaderValue(sanityShader, sanResolutionLoc, res, SHADER_UNIFORM_VEC2);
}

// ============================================================================
// Render the 3D game world
// ============================================================================
void Renderer::renderGame(const Game& game, const PlayerState& localPlayer)
{
    // Cache state for endFrame compositing
    cachedGame = &game;
    cachedPlayerState = localPlayer;
    cachedSanity = localPlayer.sanity;

    // Build camera from player state
    Camera3D camera = buildCamera(localPlayer);

    // Start rendering to the scene render texture
    BeginTextureMode(sceneTexture);
    ClearBackground({5, 3, 10, 255});

    BeginMode3D(camera);

    // Apply flashlight shader for 3D scene rendering
    if (shadersLoaded && flashlightShader.id != 0) {
        updateFlashlightUniforms(localPlayer);
        BeginShaderMode(flashlightShader);
    }

    // Render the map geometry
    renderMap();

    // Render other players
    renderPlayers(game, localPlayer.id);

    if (shadersLoaded && flashlightShader.id != 0) {
        EndShaderMode();
    }

    // Render flashlight beam visual (additive, no lighting shader)
    flashlight.update(
        GetFrameTime(),
        camera.position,
        Vector3Normalize(Vector3Subtract(camera.target, camera.position)),
        localPlayer.battery
    );
    flashlight.render();

    // Render volumetric fog particles (supplemental)
    fogSystem.update(GetFrameTime());
    fogSystem.render(camera);

    EndMode3D();
    EndTextureMode();

    // Update explored tiles for minimap
    updateExplored(localPlayer);
}

// ============================================================================
// MAP RENDERING
// ============================================================================
void Renderer::renderMap() const
{
    renderFloorAndCeiling();
    renderWalls();
    renderDoors();
    renderSwitches();
    renderItems();
}

void Renderer::renderFloorAndCeiling() const
{
    // Large floor plane
    float mapWorldW = (mapWidth > 0 ? mapWidth : GRID_WIDTH) * CELL_SIZE;
    float mapWorldH = (mapHeight > 0 ? mapHeight : GRID_HEIGHT) * CELL_SIZE;

    Color floorColor   = {45, 42, 48, 255};
    Color ceilingColor = {28, 25, 32, 255};

    // Floor at y=0
    DrawPlane(
        {mapWorldW * 0.5f, 0.0f, mapWorldH * 0.5f},
        {mapWorldW, mapWorldH},
        floorColor
    );

    // Ceiling at y=3.0 (wall height)
    DrawPlane(
        {mapWorldW * 0.5f, 3.0f, mapWorldH * 0.5f},
        {mapWorldW, mapWorldH},
        ceilingColor
    );
}

void Renderer::renderWalls() const
{
    if (!mapData) return;

    int w = (mapWidth > 0) ? mapWidth : GRID_WIDTH;
    int h = (mapHeight > 0) ? mapHeight : GRID_HEIGHT;

    Color wallColor = {65, 60, 70, 255};

    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            uint8_t tile = mapData[z * w + x];
            if (tile == TILE_WALL) {
                Vector3 pos = {
                    (x + 0.5f) * CELL_SIZE,
                    1.5f,  // center of 3.0 height wall
                    (z + 0.5f) * CELL_SIZE
                };
                DrawCube(pos, CELL_SIZE, 3.0f, CELL_SIZE, wallColor);
            }
        }
    }
}

void Renderer::renderDoors() const
{
    if (!mapData) return;

    int w = (mapWidth > 0) ? mapWidth : GRID_WIDTH;
    int h = (mapHeight > 0) ? mapHeight : GRID_HEIGHT;

    Color closedDoorColor = {90, 55, 35, 255};
    Color openDoorColor   = {60, 40, 25, 255};
    Color wallColor       = {65, 60, 70, 255};  // Also used for door frames

    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            uint8_t tile = mapData[z * w + x];
            Vector3 pos = {
                (x + 0.5f) * CELL_SIZE,
                1.5f,
                (z + 0.5f) * CELL_SIZE
            };

            if (tile == TILE_DOOR_CLOSED) {
                DrawCube(pos, CELL_SIZE, 3.0f, 0.2f, closedDoorColor);
                // Door frame
                DrawCube(
                    {pos.x - CELL_SIZE * 0.45f, 1.5f, pos.z},
                    0.1f, 3.0f, 0.3f, wallColor
                );
                DrawCube(
                    {pos.x + CELL_SIZE * 0.45f, 1.5f, pos.z},
                    0.1f, 3.0f, 0.3f, wallColor
                );
            } else if (tile == TILE_DOOR_OPEN) {
                // Open door: thin panel pushed to the side
                DrawCube(
                    {pos.x + CELL_SIZE * 0.4f, 1.5f, pos.z},
                    0.15f, 2.8f, CELL_SIZE * 0.4f,
                    openDoorColor
                );
            }
        }
    }
}

void Renderer::renderSwitches() const
{
    if (!mapData) return;

    int w = (mapWidth > 0) ? mapWidth : GRID_WIDTH;
    int h = (mapHeight > 0) ? mapHeight : GRID_HEIGHT;

    Color switchOnColor  = {30, 200, 60, 255};
    Color switchOffColor = {180, 40, 30, 255};

    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            if (mapData[z * w + x] == TILE_SWITCH) {
                Vector3 pos = {
                    (x + 0.5f) * CELL_SIZE,
                    1.4f,
                    (z + 0.5f) * CELL_SIZE
                };
                // Switch plate on wall
                DrawCube(pos, 0.15f, 0.4f, 0.3f, {80, 75, 85, 255});
                // Switch lever
                DrawCube(
                    {pos.x + 0.08f, pos.y + 0.05f, pos.z},
                    0.05f, 0.2f, 0.15f,
                    switchOnColor
                );
                // Glow effect
                DrawSphere(
                    {pos.x + 0.12f, pos.y + 0.05f, pos.z},
                    0.08f,
                    switchOnColor
                );
            }
        }
    }
}

void Renderer::renderItems() const
{
    if (!mapData) return;

    int w = (mapWidth > 0) ? mapWidth : GRID_WIDTH;
    int h = (mapHeight > 0) ? mapHeight : GRID_HEIGHT;

    Color batteryColor = {220, 200, 50, 255};
    Color keyColor     = {200, 180, 60, 255};

    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            if (mapData[z * w + x] == TILE_ITEM) {
                Vector3 pos = {
                    (x + 0.5f) * CELL_SIZE,
                    0.4f,
                    (z + 0.5f) * CELL_SIZE
                };
                // Floating item with bob animation
                float bob = sinf(animationTime * 2.5f + (float)(x * 7 + z * 13)) * 0.08f;
                pos.y += bob;

                // Item body
                DrawSphere(pos, 0.2f, batteryColor);

                // Glow ring
                DrawSphereWires(pos, 0.3f, 6, 6, {255, 240, 100, 120});
            } else if (mapData[z * w + x] == TILE_EXIT) {
                Vector3 pos = {
                    (x + 0.5f) * CELL_SIZE,
                    1.5f,
                    (z + 0.5f) * CELL_SIZE
                };
                // Exit marker - glowing pillar
                float pulse = 0.7f + 0.3f * sinf(animationTime * 3.0f);
                Color exitColor = {
                    (unsigned char)(50 * pulse),
                    (unsigned char)(200 * pulse),
                    (unsigned char)(255 * pulse),
                    255
                };
                DrawCylinder(pos, 0.15f, 0.15f, 2.8f, 8, exitColor);
                DrawSphere({pos.x, 2.9f, pos.z}, 0.25f, exitColor);
            }
        }
    }
}

// ============================================================================
// PLAYER RENDERING
// ============================================================================
void Renderer::renderPlayers(const Game& game, uint32_t localPlayerId) const
{
    const auto& players = game.getPlayers();

    for (const auto& [id, player] : players) {
        if (id == localPlayerId) continue; // Don't render local player model

        const PlayerState& state = player.getState();
        if (state.isSpectator) continue;   // Spectators are invisible

        Color bodyColor = playerColors[id % MAX_PLAYER_COLORS];
        renderPlayerModel(state.position, state.rotation, bodyColor, state.isGhost, animationTime);
    }
}

void Renderer::renderPlayerModel(Vector3 pos, float rotation, Color bodyColor, bool isGhost, float time) const
{
    // Ghost floating offset
    if (isGhost) {
        pos.y += sinf(time * 2.0f) * 0.15f + 0.1f;
    }

    // Use the matrix stack for rotation
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(rotation * RAD2DEG, 0.0f, 1.0f, 0.0f);

    // Adjust colors for ghost
    Color headColor = bodyColor;
    Color armColor  = bodyColor;
    Color legColor  = {bodyColor.r, bodyColor.g, bodyColor.b, bodyColor.a};

    if (isGhost) {
        // Semi-transparent red tinted
        bodyColor = {200, 30, 30, 120};
        headColor = {220, 50, 50, 140};
        armColor  = {180, 25, 25, 110};
        legColor  = {160, 20, 20, 100};

        // Pulsing scale effect
        float pulse = 1.0f + 0.03f * sinf(time * 3.0f);
        rlScalef(pulse, pulse, pulse);
    }

    // --- Torso ---
    DrawCylinder({0, 0.75f, 0}, 0.22f, 0.20f, 0.85f, 8, bodyColor);

    // --- Head ---
    DrawSphere({0, 1.35f, 0}, 0.18f, headColor);

    // --- Eyes (small dark spheres for non-ghosts, glowing for ghosts) ---
    if (isGhost) {
        Color eyeGlow = {255, 80, 80, 200};
        DrawSphere({-0.06f, 1.38f, 0.15f}, 0.04f, eyeGlow);
        DrawSphere({0.06f, 1.38f, 0.15f}, 0.04f, eyeGlow);
    } else {
        Color eyeColor = {20, 20, 30, 255};
        DrawSphere({-0.06f, 1.38f, 0.15f}, 0.03f, eyeColor);
        DrawSphere({0.06f, 1.38f, 0.15f}, 0.03f, eyeColor);
    }

    // --- Left Arm ---
    DrawCylinder({-0.32f, 0.70f, 0}, 0.06f, 0.05f, 0.65f, 6, armColor);

    // --- Right Arm ---
    DrawCylinder({0.32f, 0.70f, 0}, 0.06f, 0.05f, 0.65f, 6, armColor);

    // --- Left Leg ---
    DrawCylinder({-0.10f, 0.27f, 0}, 0.08f, 0.07f, 0.55f, 6, legColor);

    // --- Right Leg ---
    DrawCylinder({0.10f, 0.27f, 0}, 0.08f, 0.07f, 0.55f, 6, legColor);

    // Ghost aura
    if (isGhost) {
        float auraPulse = 0.6f + 0.4f * sinf(time * 2.5f);
        Color auraColor = {180, 20, 20, (unsigned char)(40 * auraPulse)};
        DrawSphere({0, 0.7f, 0}, 0.7f, auraColor);
    }

    rlPopMatrix();
}

// ============================================================================
// HUD RENDERING
// ============================================================================
void Renderer::renderHUD(const PlayerState& player)
{
    const int barWidth  = 200;
    const int barHeight = 18;
    const int margin    = 15;
    const int startX    = margin;
    int startY          = screenHeight - (barHeight + 8) * 3 - margin;

    // Background panel
    DrawRectangle(startX - 5, startY - 25, barWidth + 65, (barHeight + 8) * 3 + 35,
                  {0, 0, 0, 140});

    // ---- Stamina Bar (Green) ----
    float staminaPct = Clamp(player.stamina / DEFAULT_STAMINA, 0.0f, 1.0f);
    DrawText("STAMINA", startX, startY - 18, 12, {180, 180, 180, 220});
    DrawRectangle(startX, startY, barWidth, barHeight, {30, 30, 30, 200});
    Color staminaColor = {
        (unsigned char)(50 + 150 * (1.0f - staminaPct)),
        (unsigned char)(200 * staminaPct),
        50, 255
    };
    DrawRectangle(startX, startY, (int)(barWidth * staminaPct), barHeight, staminaColor);
    DrawRectangleLines(startX, startY, barWidth, barHeight, {100, 100, 100, 200});

    startY += barHeight + 8;

    // ---- Sanity Bar (Purple) ----
    float sanityPct = Clamp(player.sanity / DEFAULT_SANITY, 0.0f, 1.0f);
    DrawText("SANITY", startX, startY - 18, 12, {180, 180, 180, 220});
    DrawRectangle(startX, startY, barWidth, barHeight, {30, 30, 30, 200});
    Color sanityColor = {
        (unsigned char)(160 * sanityPct + 80),
        (unsigned char)(40 * sanityPct),
        (unsigned char)(200 * sanityPct + 55),
        255
    };
    DrawRectangle(startX, startY, (int)(barWidth * sanityPct), barHeight, sanityColor);
    DrawRectangleLines(startX, startY, barWidth, barHeight, {100, 100, 100, 200});

    startY += barHeight + 8;

    // ---- Battery Bar (Yellow) ----
    float batteryPct = Clamp(player.battery / DEFAULT_BATTERY, 0.0f, 1.0f);
    DrawText("BATTERY", startX, startY - 18, 12, {180, 180, 180, 220});
    DrawRectangle(startX, startY, barWidth, barHeight, {30, 30, 30, 200});
    Color batteryColor = {
        (unsigned char)(220 * batteryPct + 30),
        (unsigned char)(200 * batteryPct + 20),
        (unsigned char)(30),
        255
    };
    DrawRectangle(startX, startY, (int)(barWidth * batteryPct), barHeight, batteryColor);
    DrawRectangleLines(startX, startY, barWidth, barHeight, {100, 100, 100, 200});

    // Flashlight icon indicator
    const char* flashlightStatus = player.flashlightOn ? "[ON]" : "[OFF]";
    Color flashIconColor = player.flashlightOn ? (Color){220, 210, 100, 255} : (Color){120, 120, 120, 255};
    DrawText(flashlightStatus, startX + barWidth + 8, startY + 2, 14, flashIconColor);

    // ---- Dead / Ghost indicator ----
    if (player.isDead) {
        DrawText("YOU ARE DEAD", screenWidth / 2 - 80, screenHeight / 2 - 20, 30, {200, 30, 30, 220});
        DrawText("Spectating...", screenWidth / 2 - 55, screenHeight / 2 + 20, 16, {150, 150, 150, 200});
    }
    if (player.isGhost) {
        DrawText("GHOST MODE", screenWidth / 2 - 70, screenHeight / 2 - 20, 28, {200, 30, 30, 220});
    }

    // ---- FPS counter ----
    DrawFPS(screenWidth - 80, 10);
}

// ============================================================================
// SANITY EFFECTS (post-processing applied in endFrame)
// ============================================================================
void Renderer::renderSanityEffects(float sanity)
{
    // This method is called by endFrame during compositing.
    // When called standalone, it applies the sanity shader to the scene texture.
    if (!shadersLoaded || sanityShader.id == 0) return;

    updateSanityUniforms(sanity);

    BeginShaderMode(sanityShader);
    DrawTextureRec(
        sceneTexture.texture,
        {0, 0, (float)sceneTexture.texture.width, (float)-sceneTexture.texture.height},
        {0, 0},
        WHITE
    );
    EndShaderMode();
}

// ============================================================================
// CROSSHAIR
// ============================================================================
void Renderer::renderCrosshair()
{
    int cx = screenWidth / 2;
    int cy = screenHeight / 2;
    int size = 8;
    int gap  = 3;
    int thick = 2;
    Color crossColor = {220, 220, 220, 180};

    // Top
    DrawRectangle(cx - thick / 2, cy - size - gap, thick, size, crossColor);
    // Bottom
    DrawRectangle(cx - thick / 2, cy + gap, thick, size, crossColor);
    // Left
    DrawRectangle(cx - size - gap, cy - thick / 2, size, thick, crossColor);
    // Right
    DrawRectangle(cx + gap, cy - thick / 2, size, thick, crossColor);

    // Center dot
    DrawPixel(cx, cy, {255, 255, 255, 150});
}

// ============================================================================
// MINIMAP
// ============================================================================
void Renderer::renderMinimap(const Game& game, const PlayerState& localPlayer)
{
    if (!mapData || !exploredTiles) return;

    int w = (mapWidth > 0) ? mapWidth : GRID_WIDTH;
    int h = (mapHeight > 0) ? mapHeight : GRID_HEIGHT;

    const int tileSize = 3;
    const int mapPixelW = w * tileSize;
    const int mapPixelH = h * tileSize;
    const int mapX = screenWidth - mapPixelW - 15;
    const int mapY = 15;

    // Background
    DrawRectangle(mapX - 4, mapY - 4, mapPixelW + 8, mapPixelH + 8, {0, 0, 0, 180});
    DrawRectangleLines(mapX - 4, mapY - 4, mapPixelW + 8, mapPixelH + 8, {80, 80, 80, 200});

    // Draw tiles
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            int idx = z * w + x;

            // Only show explored tiles
            if (!exploredTiles[idx]) {
                DrawRectangle(mapX + x * tileSize, mapY + z * tileSize, tileSize, tileSize, {10, 8, 15, 255});
                continue;
            }

            uint8_t tile = mapData[idx];
            Color tileColor;

            switch (tile) {
                case TILE_WALL:         tileColor = {55, 50, 60, 255}; break;
                case TILE_DOOR_CLOSED:  tileColor = {100, 65, 35, 255}; break;
                case TILE_DOOR_OPEN:    tileColor = {70, 60, 50, 255}; break;
                case TILE_SWITCH:       tileColor = {30, 180, 60, 255}; break;
                case TILE_ITEM:         tileColor = {220, 200, 50, 255}; break;
                case TILE_EXIT:         tileColor = {50, 200, 255, 255}; break;
                case TILE_FLOOR:
                case TILE_SPAWN:
                default:                tileColor = {35, 32, 40, 255}; break;
            }

            DrawRectangle(mapX + x * tileSize, mapY + z * tileSize, tileSize, tileSize, tileColor);
        }
    }

    // Draw other players on the minimap
    const auto& players = game.getPlayers();
    for (const auto& [id, player] : players) {
        const PlayerState& state = player.getState();
        if (state.isSpectator) continue;

        int px = (int)(state.position.x / CELL_SIZE);
        int pz = (int)(state.position.z / CELL_SIZE);

        if (px < 0 || px >= w || pz < 0 || pz >= h) continue;

        Color pColor;
        if (state.isGhost) {
            pColor = {255, 50, 50, 255};
        } else if (id == localPlayer.id) {
            pColor = {255, 255, 255, 255};
        } else {
            pColor = playerColors[id % MAX_PLAYER_COLORS];
        }

        int dotSize = (id == localPlayer.id) ? 3 : 2;
        DrawRectangle(
            mapX + px * tileSize + (tileSize - dotSize) / 2,
            mapY + pz * tileSize + (tileSize - dotSize) / 2,
            dotSize, dotSize,
            pColor
        );
    }

    // Draw local player direction indicator
    int lpx = (int)(localPlayer.position.x / CELL_SIZE);
    int lpz = (int)(localPlayer.position.z / CELL_SIZE);
    if (lpx >= 0 && lpx < w && lpz >= 0 && lpz < h) {
        float dirX = sinf(localPlayer.rotation);
        float dirZ = cosf(localPlayer.rotation);
        int arrowStartX = mapX + lpx * tileSize + tileSize / 2;
        int arrowStartY = mapY + lpz * tileSize + tileSize / 2;
        int arrowEndX = arrowStartX + (int)(dirX * 6);
        int arrowEndY = arrowStartY + (int)(dirZ * 6);
        DrawLine(arrowStartX, arrowStartY, arrowEndX, arrowEndY, {255, 255, 255, 220});
    }
}

// ============================================================================
// INTERACTION PROMPT
// ============================================================================
void Renderer::renderInteractionPrompt(const char* text)
{
    if (!text || text[0] == '\0') return;

    int textWidth = MeasureText(text, 18);
    int posX = (screenWidth - textWidth) / 2;
    int posY = screenHeight - 80;

    // Background
    DrawRectangle(posX - 12, posY - 6, textWidth + 24, 30, {0, 0, 0, 160});
    DrawRectangleLines(posX - 12, posY - 6, textWidth + 24, 30, {180, 180, 100, 150});
    DrawText(text, posX, posY, 18, {220, 220, 180, 240});
}

// ============================================================================
// LOBBY SCREEN
// ============================================================================
void Renderer::renderLobby(const Game& game)
{
    // Dark atmospheric background
    ClearBackground({8, 5, 15, 255});

    // ---- Title ----
    const char* title = "K A I K A I";
    int titleWidth = MeasureText(title, 64);
    DrawText(title, (screenWidth - titleWidth) / 2, 60, 64, {180, 30, 50, 255});

    const char* subtitle = "Multiplayer Horror";
    int subtitleWidth = MeasureText(subtitle, 22);
    DrawText(subtitle, (screenWidth - subtitleWidth) / 2, 135, 22, {140, 120, 150, 200});

    // ---- Separator line ----
    DrawLine(screenWidth / 2 - 200, 175, screenWidth / 2 + 200, 175, {80, 60, 90, 200});

    // ---- Player list ----
    const auto& players = game.getPlayers();
    int listY = 200;

    DrawText("PLAYERS", screenWidth / 2 - 180, listY, 20, {180, 170, 190, 220});
    listY += 30;

    if (players.empty()) {
        DrawText("  No players connected...", screenWidth / 2 - 180, listY, 16, {120, 110, 130, 180});
    } else {
        for (const auto& [id, player] : players) {
            Color pColor = playerColors[id % MAX_PLAYER_COLORS];
            bool ready = game.isPlayerReady(id);
            uint32_t localId = game.getLocalPlayerId();

            // Player indicator
            DrawRectangle(screenWidth / 2 - 180, listY, 12, 12, pColor);

            // Player name
            char nameBuf[64];
            snprintf(nameBuf, sizeof(nameBuf), "Player %u", id);
            Color nameColor = (id == localId) ? (Color){255, 255, 200, 255} : (Color){200, 195, 210, 230};
            DrawText(nameBuf, screenWidth / 2 - 162, listY - 2, 16, nameColor);

            // Ready status
            const char* statusText = ready ? "READY" : "NOT READY";
            Color statusColor = ready ? (Color){80, 220, 80, 255} : (Color){180, 80, 80, 255};
            DrawText(statusText, screenWidth / 2 + 80, listY - 2, 16, statusColor);

            listY += 26;
        }
    }

    // ---- Waiting message ----
    int waitY = screenHeight - 120;
    float pulse = 0.6f + 0.4f * sinf(animationTime * 2.0f);
    Color waitColor = {
        (unsigned char)(150 * pulse),
        (unsigned char)(140 * pulse),
        (unsigned char)(170 * pulse),
        255
    };
    const char* waitText = "Waiting for players...";
    int waitWidth = MeasureText(waitText, 20);
    DrawText(waitText, (screenWidth - waitWidth) / 2, waitY, 20, waitColor);

    // ---- Instructions ----
    const char* instr1 = "Press [R] to ready up";
    const char* instr2 = "Press [ENTER] to start (host only)";
    int instr1W = MeasureText(instr1, 14);
    int instr2W = MeasureText(instr2, 14);
    DrawText(instr1, (screenWidth - instr1W) / 2, screenHeight - 60, 14, {120, 115, 135, 200});
    DrawText(instr2, (screenWidth - instr2W) / 2, screenHeight - 38, 14, {100, 95, 115, 180});
}

// ============================================================================
// GAME OVER SCREEN
// ============================================================================
void Renderer::renderGameOver(bool survivorsWon)
{
    // Full screen dark overlay
    DrawRectangle(0, 0, screenWidth, screenHeight, {0, 0, 0, 200});

    if (survivorsWon) {
        // Survivors win
        const char* winText = "SURVIVORS ESCAPED";
        int winWidth = MeasureText(winText, 48);
        DrawText(winText, (screenWidth - winWidth) / 2, screenHeight / 2 - 60, 48, {80, 230, 100, 255});

        const char* subText = "The nightmare is over... for now.";
        int subWidth = MeasureText(subText, 20);
        DrawText(subText, (screenWidth - subWidth) / 2, screenHeight / 2, 20, {120, 200, 130, 200});
    } else {
        // Ghost wins
        const char* loseText = "THE GHOST PREVAILS";
        int loseWidth = MeasureText(loseText, 48);
        DrawText(loseText, (screenWidth - loseWidth) / 2, screenHeight / 2 - 60, 48, {220, 40, 40, 255});

        const char* subText = "Darkness consumes all...";
        int subWidth = MeasureText(subText, 20);
        DrawText(subText, (screenWidth - subWidth) / 2, screenHeight / 2, 20, {180, 60, 60, 200});
    }

    // Return to lobby instruction
    float pulse = 0.5f + 0.5f * sinf(animationTime * 2.5f);
    const char* returnText = "Press [ENTER] to return to lobby";
    int returnWidth = MeasureText(returnText, 18);
    Color returnColor = {
        (unsigned char)(180 * pulse + 40),
        (unsigned char)(170 * pulse + 40),
        (unsigned char)(190 * pulse + 40),
        255
    };
    DrawText(returnText, (screenWidth - returnWidth) / 2, screenHeight / 2 + 60, 18, returnColor);
}

// ============================================================================
// Update explored tiles for minimap
// ============================================================================
void Renderer::updateExplored(const PlayerState& localPlayer)
{
    if (!exploredTiles || !mapData) return;

    int w = (mapWidth > 0) ? mapWidth : GRID_WIDTH;
    int h = (mapHeight > 0) ? mapHeight : GRID_HEIGHT;

    // Player tile position
    int px = (int)(localPlayer.position.x / CELL_SIZE);
    int pz = (int)(localPlayer.position.z / CELL_SIZE);

    // Reveal tiles within a view radius
    const int viewRadius = 6;

    for (int dz = -viewRadius; dz <= viewRadius; ++dz) {
        for (int dx = -viewRadius; dx <= viewRadius; ++dx) {
            int tx = px + dx;
            int tz = pz + dz;

            if (tx < 0 || tx >= w || tz < 0 || tz >= h) continue;

            // Circular falloff
            float dist = sqrtf((float)(dx * dx + dz * dz));
            if (dist > viewRadius) continue;

            exploredTiles[tz * w + tx] = true;
        }
    }
}
