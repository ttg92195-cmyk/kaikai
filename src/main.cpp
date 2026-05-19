#include "raylib.h"
#include "raymath.h"
#include "game/Game.h"
#include "graphics/Renderer.h"
#include "graphics/Flashlight.h"
#include "graphics/FogSystem.h"
#include "networking/Server.h"
#include "networking/Client.h"
#include "ai/GhostAI.h"
#include "ai/Enemy.h"
#include "systems/StaminaSystem.h"
#include "systems/SanitySystem.h"
#include "systems/JumpscareSystem.h"
#include "systems/SpectatorSystem.h"
#include "systems/ItemSpawnSystem.h"
#include "audio/AudioManager.h"
#include "utils/Constants.h"
#include "utils/Logger.h"

#include <iostream>
#include <string>
#include <csignal>
#include <memory>
#include <cmath>

using namespace Kaikai;

// Global for signal handling
static bool g_running = true;

void signalHandler(int signum) {
    g_running = false;
}

// Helper: compute forward direction vector from a rotation (yaw) value
static Vector3 computeForwardVector(float rotation) {
    // rotation is in radians (yaw around Y axis)
    return {
        sinf(rotation),
        0.0f,
        cosf(rotation)
    };
}

int main(int argc, char* argv[]) {
    // Parse command line args
    bool isServer = false;
    std::string serverIP = "127.0.0.1";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--server" || arg == "-s") isServer = true;
        else if ((arg == "--connect" || arg == "-c") && i + 1 < argc) serverIP = argv[++i];
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: kaikai [options]\n"
                      << "  --server, -s       Run as server\n"
                      << "  --connect, -c IP   Connect to server at IP\n"
                      << "  --help, -h         Show this help\n";
            return 0;
        }
    }

    signal(SIGINT, signalHandler);
    LOG_INFO("Kaikai - 3D Multiplayer Horror Game");
    LOG_INFO(isServer ? "Starting as SERVER" : "Starting as CLIENT");

    if (isServer) {
        // ===== SERVER MODE =====
        Server server;
        if (!server.start(SERVER_PORT)) {
            LOG_ERROR("Failed to start server!");
            return 1;
        }
        LOG_INFO("Server started on port %d", SERVER_PORT);

        float deltaTime = 0.0f;
        while (g_running) {
            float frameStart = (float)GetTime();
            server.update(deltaTime);
            deltaTime = (float)(GetTime() - frameStart);
            if (deltaTime < 1.0f / TICK_RATE) {
                // Busy wait for consistent tick rate
                while ((float)(GetTime() - frameStart) < 1.0f / TICK_RATE) {}
                deltaTime = 1.0f / TICK_RATE;
            }
        }
        server.stop();
        LOG_INFO("Server shut down.");
    } else {
        // ===== CLIENT MODE =====
        // Initialize renderer
        Renderer renderer;
        renderer.init(1280, 720);

        // Initialize systems
        Client client;
        AudioManager audioManager;
        Flashlight flashlight;
        FogSystem fogSystem;
        StaminaSystem staminaSystem;
        SanitySystem sanitySystem;
        JumpscareSystem jumpscareSystem;
        SpectatorSystem spectatorSystem;
        ItemSpawnSystem itemSpawnSystem;

        audioManager.init();

        // Connect to server
        if (!client.connect(serverIP.c_str(), SERVER_PORT)) {
            LOG_ERROR("Failed to connect to server at %s:%d", serverIP.c_str(), SERVER_PORT);
            renderer.shutdown();
            audioManager.shutdown();
            return 1;
        }
        LOG_INFO("Connected to server at %s:%d", serverIP.c_str(), SERVER_PORT);

        // Game state
        Game game(false); // client mode

        float deltaTime = 0.0f;
        SetTargetFPS((int)TICK_RATE);

        while (g_running && !WindowShouldClose()) {
            deltaTime = GetFrameTime();

            // Update network
            client.update(deltaTime);

            // Get local player
            uint32_t localId = client.getLocalPlayerId();
            Player* localPlayer = game.getPlayer(localId);

            if (localPlayer) {
                PlayerState& state = localPlayer->getStateMut();

                // Handle input
                localPlayer->handleInput(deltaTime);

                // Update systems
                staminaSystem.update(deltaTime, state);
                sanitySystem.update(deltaTime, state, !state.flashlightOn, 100.0f); // ghost distance placeholder

                // Compute proper forward vector from player rotation for flashlight
                Vector3 forward = computeForwardVector(state.rotation);
                flashlight.update(deltaTime, state.position, forward, state.battery);

                audioManager.updateGhostProximity(100.0f); // placeholder
                audioManager.updateFootstepSounds(deltaTime, state);

                // Check jumpscare
                if (jumpscareSystem.checkDynamicTrigger(state, deltaTime, false, state.sanity)) {
                    jumpscareSystem.triggerJumpscare(localId, state.position);
                }
                jumpscareSystem.update(deltaTime);

                // Update fog
                fogSystem.update(deltaTime);

                // Send position to server
                client.sendPlayerMove(state);
            }

            // Spectator mode for dead players
            if (localPlayer && localPlayer->getState().isDead) {
                // Build the list of alive players from the game state
                std::vector<PlayerState> alivePlayers;
                const auto& allPlayers = game.getPlayers();
                for (const auto& [pid, player] : allPlayers) {
                    if (!player.getState().isDead && !player.getState().isSpectator) {
                        alivePlayers.push_back(player.getState());
                    }
                }
                spectatorSystem.update(deltaTime, alivePlayers);
            }

            // Render
            renderer.beginFrame();
            if (localPlayer) {
                renderer.renderGame(game, localPlayer->getState());
                renderer.renderHUD(localPlayer->getState());
                renderer.renderSanityEffects(localPlayer->getState().sanity);
            }

            // Build camera for jumpscare rendering
            if (localPlayer) {
                const PlayerState& ps = localPlayer->getState();
                Camera3D camera = { 0 };
                camera.position = ps.position;
                camera.position.y += Kaikai::PLAYER_HEIGHT * 0.9f; // Eye level
                Vector3 fwd = computeForwardVector(ps.rotation);
                camera.target = Vector3Add(camera.position, fwd);
                camera.up = { 0.0f, 1.0f, 0.0f };
                camera.fovy = 60.0f;
                camera.projection = CAMERA_PERSPECTIVE;
                jumpscareSystem.render(camera);
            }

            renderer.endFrame();
        }

        client.disconnect();
        audioManager.shutdown();
        renderer.shutdown();
        LOG_INFO("Client shut down.");
    }

    return 0;
}
