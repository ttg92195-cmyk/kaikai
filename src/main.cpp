#if defined(KAIKAI_HEADLESS)
// Headless server — no raylib, no graphics, no audio
#include "networking/Server.h"
#include "ai/GhostAI.h"
#include "ai/Enemy.h"
#include "systems/StaminaSystem.h"
#include "systems/SanitySystem.h"
#include "systems/JumpscareSystem.h"
#include "systems/SpectatorSystem.h"
#include "systems/ItemSpawnSystem.h"
#include "utils/Constants.h"
#include "utils/Logger.h"
#include <iostream>
#include <string>
#include <csignal>
#include <memory>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#else
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

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include <iostream>
#include <string>
#include <csignal>
#include <memory>
#include <cmath>
#include <cstring>
#endif

using namespace Kaikai;

// ============================================================================
// Android Touch Input System
// ============================================================================
#if defined(__ANDROID__) && !defined(KAIKAI_HEADLESS)

// Virtual joystick state
struct VirtualJoystick {
    Vector2 center = {0, 0};
    Vector2 current = {0, 0};
    float radius = 0.0f;
    int touchId = -1;       // which touch is controlling this joystick (-1 = none)
    bool active = false;
};

// Touch button state
struct TouchButton {
    Rectangle bounds = {0, 0, 0, 0};
    bool pressed = false;
    int touchId = -1;
    const char* label = "";
};

// Android input manager
struct AndroidInput {
    VirtualJoystick moveJoystick;
    TouchButton btnRun;
    TouchButton btnFlashlight;
    TouchButton btnInteract;
    Vector2 lookDelta = {0, 0};
    int lookTouchId = -1;
    Vector2 lastLookPos = {0, 0};

    // UI layout dimensions (set during init)
    float joystickRadius = 0.0f;
    float buttonSize = 0.0f;
    int screenWidth = 0;
    int screenHeight = 0;
    bool layoutInitialized = false;
};

static AndroidInput androidInput = {};

void initAndroidLayout(int screenWidth, int screenHeight) {
    androidInput.screenWidth = screenWidth;
    androidInput.screenHeight = screenHeight;
    androidInput.joystickRadius = screenHeight * 0.12f;
    androidInput.buttonSize = screenHeight * 0.09f;

    // Move joystick: bottom-left area
    androidInput.moveJoystick.center = {
        screenHeight * 0.22f,
        (float)screenHeight - screenHeight * 0.22f
    };
    androidInput.moveJoystick.radius = androidInput.joystickRadius;

    // Run button: bottom-center-left
    androidInput.btnRun.bounds = {
        screenHeight * 0.38f,
        (float)screenHeight - androidInput.buttonSize - screenHeight * 0.04f,
        androidInput.buttonSize * 1.3f,
        androidInput.buttonSize
    };
    androidInput.btnRun.label = "RUN";

    // Flashlight button: bottom-right
    androidInput.btnFlashlight.bounds = {
        (float)screenWidth - androidInput.buttonSize * 1.3f - screenHeight * 0.04f,
        (float)screenHeight - androidInput.buttonSize - screenHeight * 0.04f,
        androidInput.buttonSize * 1.3f,
        androidInput.buttonSize
    };
    androidInput.btnFlashlight.label = "LIGHT";

    // Interact button: bottom-center-right
    androidInput.btnInteract.bounds = {
        (float)screenWidth - androidInput.buttonSize * 2.7f - screenHeight * 0.08f,
        (float)screenHeight - androidInput.buttonSize - screenHeight * 0.04f,
        androidInput.buttonSize * 1.3f,
        androidInput.buttonSize
    };
    androidInput.btnInteract.label = "USE";

    androidInput.layoutInitialized = true;
}

void updateAndroidTouch() {
    // Reset per-frame state
    androidInput.lookDelta = {0, 0};

    // Reset button pressed state (will be set by touch)
    androidInput.btnRun.pressed = false;
    androidInput.btnFlashlight.pressed = false;
    androidInput.btnInteract.pressed = false;

    int touchCount = GetTouchPointCount();

    for (int i = 0; i < touchCount && i < 8; i++) {
        Vector2 pos = GetTouchPosition(i);
        int id = i; // Use index as touch ID

        // Check if this touch is on the move joystick area (left half)
        bool inJoystickArea = (pos.x < androidInput.screenWidth * 0.45f &&
                               pos.y > androidInput.screenHeight * 0.4f);

        // Check if this touch is on button areas
        bool inRunBtn = CheckCollisionPointRec(pos, androidInput.btnRun.bounds);
        bool inFlashBtn = CheckCollisionPointRec(pos, androidInput.btnFlashlight.bounds);
        bool inInteractBtn = CheckCollisionPointRec(pos, androidInput.btnInteract.bounds);

        if (inJoystickArea && !inRunBtn) {
            if (androidInput.moveJoystick.touchId == -1 ||
                androidInput.moveJoystick.touchId == id) {
                androidInput.moveJoystick.touchId = id;
                androidInput.moveJoystick.current = pos;
                androidInput.moveJoystick.active = true;
            }
        } else if (inRunBtn) {
            androidInput.btnRun.pressed = true;
            androidInput.btnRun.touchId = id;
        } else if (inFlashBtn) {
            // Only trigger on new touch (not held)
            if (androidInput.btnFlashlight.touchId != id) {
                androidInput.btnFlashlight.pressed = true;
            }
            androidInput.btnFlashlight.touchId = id;
        } else if (inInteractBtn) {
            androidInput.btnInteract.pressed = true;
            androidInput.btnInteract.touchId = id;
        } else {
            // This touch is in the look area (right half, upper area)
            if (androidInput.lookTouchId == -1 ||
                androidInput.lookTouchId == id) {
                if (androidInput.lookTouchId == -1) {
                    // New look touch
                    androidInput.lookTouchId = id;
                    androidInput.lastLookPos = pos;
                } else {
                    // Continuing look touch - calculate delta
                    androidInput.lookDelta.x = pos.x - androidInput.lastLookPos.x;
                    androidInput.lookDelta.y = pos.y - androidInput.lastLookPos.y;
                    androidInput.lastLookPos = pos;
                }
            }
        }
    }

    // Clean up released touches
    // If a touch ID is no longer in the touch list, reset it
    bool moveJoystickFound = false;
    bool lookTouchFound = false;
    bool flashBtnFound = false;

    for (int i = 0; i < touchCount && i < 8; i++) {
        if (i == androidInput.moveJoystick.touchId) moveJoystickFound = true;
        if (i == androidInput.lookTouchId) lookTouchFound = true;
        if (i == androidInput.btnFlashlight.touchId) flashBtnFound = true;
    }

    if (!moveJoystickFound) {
        androidInput.moveJoystick.touchId = -1;
        androidInput.moveJoystick.active = false;
        androidInput.moveJoystick.current = androidInput.moveJoystick.center;
    }

    if (!lookTouchFound) {
        androidInput.lookTouchId = -1;
        androidInput.lookDelta = {0, 0};
    }

    if (!flashBtnFound) {
        androidInput.btnFlashlight.touchId = -1;
    }
}

// Get movement vector from virtual joystick (-1 to 1 range)
Vector2 getAndroidMoveVector() {
    if (!androidInput.moveJoystick.active) return {0, 0};

    float dx = androidInput.moveJoystick.current.x - androidInput.moveJoystick.center.x;
    float dy = androidInput.moveJoystick.current.y - androidInput.moveJoystick.center.y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < androidInput.joystickRadius * 0.1f) return {0, 0}; // Dead zone

    float clampDist = (dist < androidInput.joystickRadius) ? dist : androidInput.joystickRadius;
    float norm = clampDist / androidInput.joystickRadius;

    return {
        (dx / dist) * norm,
        (dy / dist) * norm
    };
}

// Draw touch controls overlay
void drawAndroidControls() {
    if (!androidInput.layoutInitialized) return;

    // ---- Move Joystick ----
    // Outer circle
    DrawCircleV(androidInput.moveJoystick.center, androidInput.joystickRadius,
                {255, 255, 255, 40});
    DrawCircleLines(androidInput.moveJoystick.center.x,
                    androidInput.moveJoystick.center.y,
                    androidInput.joystickRadius,
                    {255, 255, 255, 100});

    // Inner thumb
    Vector2 thumbPos = androidInput.moveJoystick.center;
    if (androidInput.moveJoystick.active) {
        float dx = androidInput.moveJoystick.current.x - androidInput.moveJoystick.center.x;
        float dy = androidInput.moveJoystick.current.y - androidInput.moveJoystick.center.y;
        float dist = sqrtf(dx * dx + dy * dy);
        float maxDist = androidInput.joystickRadius;
        if (dist > maxDist) {
            thumbPos.x += (dx / dist) * maxDist;
            thumbPos.y += (dy / dist) * maxDist;
        } else {
            thumbPos = androidInput.moveJoystick.current;
        }
    }
    DrawCircleV(thumbPos, androidInput.joystickRadius * 0.45f,
                {255, 255, 255, 120});

    // ---- Buttons ----
    auto drawBtn = [](const TouchButton& btn, Color normalColor) {
        Color bg = btn.pressed ? (Color){normalColor.r, normalColor.g, normalColor.b, 200}
                               : (Color){normalColor.r, normalColor.g, normalColor.b, 80};
        DrawRectangleRec(btn.bounds, bg);
        DrawRectangleLinesEx(btn.bounds, 2, {255, 255, 255, 150});

        int textWidth = MeasureText(btn.label, (int)(androidInput.buttonSize * 0.4f));
        int textX = (int)(btn.bounds.x + (btn.bounds.width - textWidth) / 2);
        int textY = (int)(btn.bounds.y + (btn.bounds.height - androidInput.buttonSize * 0.4f) / 2);
        DrawText(btn.label, textX, textY,
                 (int)(androidInput.buttonSize * 0.4f), {255, 255, 255, 220});
    };

    drawBtn(androidInput.btnRun, {80, 180, 80});
    drawBtn(androidInput.btnFlashlight, {220, 200, 50});
    drawBtn(androidInput.btnInteract, {80, 140, 220});
}

#endif // __ANDROID__

// ============================================================================
// Game States (for Android menu flow)
// ============================================================================
enum class AppState {
    MENU,           // Title / menu screen
    CONNECTING,     // Attempting to connect to server
    PLAYING,        // In-game
    CONNECTION_FAILED // Connection error screen
};

// Global for signal handling (desktop)
static bool g_running = true;

void signalHandler(int signum) {
    g_running = false;
}

// Helper: compute forward direction vector from a rotation (yaw) value
#if !defined(KAIKAI_HEADLESS)
static Vector3 computeForwardVector(float rotation) {
    return {
        sinf(rotation),
        0.0f,
        cosf(rotation)
    };
}
#endif

// ============================================================================
// Draw the main menu / title screen
// ============================================================================
#if !defined(KAIKAI_HEADLESS)
void drawMenuScreen(int screenWidth, int screenHeight, float time, const char* statusText) {
    BeginDrawing();
    ClearBackground({8, 5, 15, 255});

    // Title with pulsing effect
    float titlePulse = 0.85f + 0.15f * sinf(time * 1.5f);
    const char* title = "K A I K A I";
    int titleWidth = MeasureText(title, 64);
    Color titleColor = {
        (unsigned char)(180 * titlePulse),
        (unsigned char)(30 * titlePulse),
        (unsigned char)(50 * titlePulse),
        255
    };
    DrawText(title, (screenWidth - titleWidth) / 2, screenHeight / 4, 64, titleColor);

    const char* subtitle = "Multiplayer Horror";
    int subtitleWidth = MeasureText(subtitle, 22);
    DrawText(subtitle, (screenWidth - subtitleWidth) / 2, screenHeight / 4 + 75, 22,
             {140, 120, 150, 200});

    // Separator
    DrawLine(screenWidth / 2 - 200, screenHeight / 4 + 110,
             screenWidth / 2 + 200, screenHeight / 4 + 110,
             {80, 60, 90, 200});

    // Status text / instructions
    if (statusText) {
        int statusWidth = MeasureText(statusText, 20);
        float statusPulse = 0.6f + 0.4f * sinf(time * 2.5f);
        Color statusColor = {
            (unsigned char)(200 * statusPulse),
            (unsigned char)(190 * statusPulse),
            (unsigned char)(210 * statusPulse),
            255
        };
        DrawText(statusText, (screenWidth - statusWidth) / 2, screenHeight / 2 + 20, 20, statusColor);
    }

#if defined(__ANDROID__)
    const char* actionText = "Tap anywhere to connect to server";
    int actionWidth = MeasureText(actionText, 24);
    float actionPulse = 0.5f + 0.5f * sinf(time * 3.0f);
    Color actionColor = {
        (unsigned char)(220 * actionPulse + 30),
        (unsigned char)(200 * actionPulse + 30),
        (unsigned char)(100 * actionPulse + 30),
        255
    };
    DrawText(actionText, (screenWidth - actionWidth) / 2, screenHeight * 3 / 4, 24, actionColor);

    const char* hint = "Server must be running on the same WiFi network at port 7777";
    int hintWidth = MeasureText(hint, 14);
    DrawText(hint, (screenWidth - hintWidth) / 2, screenHeight * 3 / 4 + 40, 14,
             {120, 115, 135, 180});
#else
    const char* instr1 = "Press [ENTER] to start as client (connects to localhost)";
    const char* instr2 = "Press [S] to start as server";
    const char* instr3 = "Press [C] + type IP to connect to specific server";
    int w1 = MeasureText(instr1, 16);
    int w2 = MeasureText(instr2, 16);
    int w3 = MeasureText(instr3, 16);
    DrawText(instr1, (screenWidth - w1) / 2, screenHeight * 3 / 4, 16, {180, 180, 180, 200});
    DrawText(instr2, (screenWidth - w2) / 2, screenHeight * 3 / 4 + 25, 16, {180, 180, 180, 200});
    DrawText(instr3, (screenWidth - w3) / 2, screenHeight * 3 / 4 + 50, 16, {180, 180, 180, 200});
#endif

    EndDrawing();
}
#endif // !KAIKAI_HEADLESS

// ============================================================================
// Draw connection failed screen
// ============================================================================
#if !defined(KAIKAI_HEADLESS)
void drawConnectionFailedScreen(int screenWidth, int screenHeight, float time,
                                 const char* serverIP) {
    BeginDrawing();
    ClearBackground({15, 5, 8, 255});

    const char* failText = "CONNECTION FAILED";
    int failWidth = MeasureText(failText, 40);
    DrawText(failText, (screenWidth - failWidth) / 2, screenHeight / 3, 40,
             {220, 50, 50, 255});

    char ipBuf[128];
    snprintf(ipBuf, sizeof(ipBuf), "Could not connect to: %s:%d", serverIP, SERVER_PORT);
    int ipWidth = MeasureText(ipBuf, 18);
    DrawText(ipBuf, (screenWidth - ipWidth) / 2, screenHeight / 3 + 60, 18,
             {180, 150, 150, 200});

    const char* hint1 = "Make sure the server is running on the same network";
    const char* hint2 = "The server IP must be reachable from this device";
    int w1 = MeasureText(hint1, 16);
    int w2 = MeasureText(hint2, 16);
    DrawText(hint1, (screenWidth - w1) / 2, screenHeight / 2 + 20, 16,
             {140, 130, 130, 180});
    DrawText(hint2, (screenWidth - w2) / 2, screenHeight / 2 + 45, 16,
             {140, 130, 130, 180});

#if defined(__ANDROID__)
    const char* retryText = "Tap anywhere to retry";
    float retryPulse = 0.5f + 0.5f * sinf(time * 3.0f);
    int retryWidth = MeasureText(retryText, 24);
    Color retryColor = {
        (unsigned char)(220 * retryPulse + 30),
        (unsigned char)(180 * retryPulse + 30),
        (unsigned char)(80 * retryPulse + 30),
        255
    };
    DrawText(retryText, (screenWidth - retryWidth) / 2, screenHeight * 3 / 4, 24, retryColor);
#else
    const char* retryText = "Press [ENTER] to retry or [ESC] to quit";
    int retryWidth = MeasureText(retryText, 18);
    DrawText(retryText, (screenWidth - retryWidth) / 2, screenHeight * 3 / 4, 18,
             {180, 180, 180, 200});
#endif

    EndDrawing();
}
#endif // !KAIKAI_HEADLESS

// ============================================================================
// Android-specific: Handle touch input for player movement
// ============================================================================
#if defined(__ANDROID__) && !defined(KAIKAI_HEADLESS)
void handleAndroidPlayerInput(Player* player, float deltaTime) {
    if (!player) return;

    updateAndroidTouch();

    const PlayerState& state = player->getState();
    if (state.isDead || state.isSpectator) return;

    // --- Look (from swipe on right side) ---
    Vector2 lookD = androidInput.lookDelta;
    float sensitivity = 0.005f;
    player->rotate(lookD.x * sensitivity);

    // Pitch (vertical look)
    // We can't directly set pitch since it's private in Player,
    // but we can adjust rotation for horizontal look only which is the primary control

    // --- Movement (from virtual joystick) ---
    Vector2 moveVec = getAndroidMoveVector();
    float moveX = moveVec.x;  // Left/Right
    float moveY = moveVec.y;  // Forward/Backward (inverted: negative Y = forward)

    // Forward/backward based on joystick Y axis
    if (moveY < -0.15f) {
        player->moveForward(deltaTime * fabsf(moveY));
    }
    if (moveY > 0.15f) {
        player->moveBackward(deltaTime * fabsf(moveY));
    }
    // Left/right based on joystick X axis
    if (moveX < -0.15f) {
        player->moveLeft(deltaTime * fabsf(moveX));
    }
    if (moveX > 0.15f) {
        player->moveRight(deltaTime * fabsf(moveX));
    }

    // --- Run button ---
    player->toggleRun(androidInput.btnRun.pressed);

    // --- Flashlight button (toggle on press) ---
    if (androidInput.btnFlashlight.pressed) {
        player->toggleFlashlight();
    }
}
#endif

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    // Parse command line args
    bool isServer = false;
    std::string serverIP = "127.0.0.1";

#if defined(KAIKAI_HEADLESS)
    // Headless build can only run as server
    (void)argc;
    (void)argv;
    isServer = true;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "--connect" || arg == "-c") && i + 1 < argc) serverIP = argv[++i];
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: kaikai_headless_server [options]\n"
                      << "  (Always runs as server)\n"
                      << "  --connect, -c IP   Set server bind IP (default: 127.0.0.1)\n"
                      << "  --help, -h         Show this help\n";
            return 0;
        }
    }
    signal(SIGINT, signalHandler);
#elif defined(__ANDROID__)
    // On Android, always run as client
    (void)argc;
    (void)argv;
    isServer = false;
    // Default server IP: try to find server on local network
    // User can change this in the menu
    serverIP = "192.168.1.100"; // Placeholder - will be configurable
#else
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
#endif

    LOG_INFO("Kaikai - 3D Multiplayer Horror Game");
    LOG_INFO(isServer ? "Starting as SERVER" : "Starting as CLIENT");

    // ========================================================================
    // SERVER MODE
    // ========================================================================
    if (isServer) {
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
#if defined(KAIKAI_HEADLESS)
                // Use sleep instead of busy-wait in headless mode to save CPU
                float remaining = 1.0f / TICK_RATE - deltaTime;
                if (remaining > 0.001f) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<float>(remaining * 0.9f));
                }
#else
                while ((float)(GetTime() - frameStart) < 1.0f / TICK_RATE) {}
#endif
                deltaTime = 1.0f / TICK_RATE;
            }
        }
        server.stop();
        LOG_INFO("Server shut down.");
        return 0;
    }

#if defined(KAIKAI_HEADLESS)
    // Headless build should never reach here (isServer is always true)
    LOG_ERROR("Headless build cannot run as client.");
    return 1;
#else

    // ========================================================================
    // CLIENT MODE
    // ========================================================================
#if defined(__ANDROID__)
    // --- Android Client: Start with menu screen ---
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Kaikai - Multiplayer Horror");

    // Safety: check that window was created successfully
    if (!IsWindowReady()) {
        LOG_ERROR("Failed to initialize window on Android!");
        return 1;
    }

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // Safety: ensure valid screen dimensions
    if (screenWidth <= 0 || screenHeight <= 0) {
        screenWidth = 1280;
        screenHeight = 720;
    }

    LOG_INFO("Android screen: %dx%d", screenWidth, screenHeight);

    // Initialize Android touch controls layout
    initAndroidLayout(screenWidth, screenHeight);

    SetTargetFPS(60);

    // Application state machine
    AppState appState = AppState::MENU;
    float menuTime = 0.0f;

    // Game objects (created when entering PLAYING state)
    Renderer* renderer = nullptr;
    Client* client = nullptr;
    AudioManager* audioManager = nullptr;
    Flashlight* flashlight = nullptr;
    FogSystem* fogSystem = nullptr;
    StaminaSystem* staminaSystem = nullptr;
    SanitySystem* sanitySystem = nullptr;
    JumpscareSystem* jumpscareSystem = nullptr;
    SpectatorSystem* spectatorSystem = nullptr;
    ItemSpawnSystem* itemSpawnSystem = nullptr;
    Game* game = nullptr;

    // Touch state tracking for tap detection (no static variables in switch)
    bool prevTouching = false;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Clamp deltaTime to prevent huge jumps (e.g., after loading)
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        bool touching = (GetTouchPointCount() > 0);
        bool tapDetected = (prevTouching && !touching); // Released = tap
        prevTouching = touching;

        // Check for resize
        int newW = GetScreenWidth();
        int newH = GetScreenHeight();
        if (newW != screenWidth || newH != screenHeight) {
            screenWidth = newW;
            screenHeight = newH;
            if (screenWidth > 0 && screenHeight > 0) {
                initAndroidLayout(screenWidth, screenHeight);
            }
        }

        switch (appState) {
            // ---- MENU SCREEN ----
            case AppState::MENU: {
                menuTime += deltaTime;

                if (tapDetected || IsKeyPressed(KEY_ENTER)) {
                    appState = AppState::CONNECTING;
                    menuTime = 0.0f;
                }

                drawMenuScreen(screenWidth, screenHeight, menuTime,
                              "Ready to enter the nightmare...");
                break;
            }

            // ---- CONNECTING ----
            case AppState::CONNECTING: {
                menuTime += deltaTime;

                // Show connecting screen
                drawMenuScreen(screenWidth, screenHeight, menuTime,
                              "Connecting to server...");

                // Attempt connection - with safety wrapping for each component
                bool initSuccess = true;

                // 1. Create and init Renderer
                try {
                    renderer = new Renderer();
                    renderer->init(screenWidth, screenHeight);
                } catch (...) {
                    LOG_ERROR("Failed to initialize renderer!");
                    delete renderer; renderer = nullptr;
                    initSuccess = false;
                }

                // 2. Create and init AudioManager
                if (initSuccess) {
                    try {
                        audioManager = new AudioManager();
                        audioManager->init();
                        // Audio init failure is not fatal - game can run without audio
                    } catch (...) {
                        LOG_ERROR("Failed to initialize audio!");
                        delete audioManager; audioManager = nullptr;
                        // Not fatal - continue without audio
                    }
                }

                // 3. Create Client and connect
                if (initSuccess) {
                    try {
                        client = new Client();
                    } catch (...) {
                        LOG_ERROR("Failed to create network client!");
                        delete client; client = nullptr;
                        initSuccess = false;
                    }
                }

                if (initSuccess && client) {
                    if (!client->connect(serverIP.c_str(), SERVER_PORT)) {
                        LOG_ERROR("Failed to connect to server at %s:%d", serverIP.c_str(), SERVER_PORT);
                        appState = AppState::CONNECTION_FAILED;
                        menuTime = 0.0f;

                        // Cleanup on connection failure
                        delete client; client = nullptr;
                        if (audioManager) { audioManager->shutdown(); delete audioManager; audioManager = nullptr; }
                        if (renderer) { renderer->shutdown(); delete renderer; renderer = nullptr; }
                    } else {
                        LOG_INFO("Connected to server at %s:%d", serverIP.c_str(), SERVER_PORT);
                        appState = AppState::PLAYING;

                        // Create game systems - with safety
                        try {
                            flashlight = new Flashlight();
                            fogSystem = new FogSystem();
                            staminaSystem = new StaminaSystem();
                            sanitySystem = new SanitySystem();
                            jumpscareSystem = new JumpscareSystem();
                            spectatorSystem = new SpectatorSystem();
                            itemSpawnSystem = new ItemSpawnSystem();
                            game = new Game(false);
                        } catch (...) {
                            LOG_ERROR("Failed to initialize game systems!");
                            // Clean up whatever was created
                            delete game; game = nullptr;
                            delete itemSpawnSystem; itemSpawnSystem = nullptr;
                            delete spectatorSystem; spectatorSystem = nullptr;
                            delete jumpscareSystem; jumpscareSystem = nullptr;
                            delete sanitySystem; sanitySystem = nullptr;
                            delete staminaSystem; staminaSystem = nullptr;
                            delete fogSystem; fogSystem = nullptr;
                            delete flashlight; flashlight = nullptr;

                            if (client) { client->disconnect(); delete client; client = nullptr; }
                            if (audioManager) { audioManager->shutdown(); delete audioManager; audioManager = nullptr; }
                            if (renderer) { renderer->shutdown(); delete renderer; renderer = nullptr; }

                            appState = AppState::CONNECTION_FAILED;
                            menuTime = 0.0f;
                        }
                    }
                } else if (!initSuccess) {
                    // Initialization failed - go back to connection failed screen
                    appState = AppState::CONNECTION_FAILED;
                    menuTime = 0.0f;

                    if (client) { delete client; client = nullptr; }
                    if (audioManager) { audioManager->shutdown(); delete audioManager; audioManager = nullptr; }
                    if (renderer) { renderer->shutdown(); delete renderer; renderer = nullptr; }
                }
                break;
            }

            // ---- PLAYING ----
            case AppState::PLAYING: {
                if (!client || !client->isConnected() || !renderer || !game) {
                    appState = AppState::CONNECTION_FAILED;
                    menuTime = 0.0f;

                    // Safe cleanup with null checks
                    if (client) { client->disconnect(); delete client; client = nullptr; }
                    delete game; game = nullptr;
                    delete itemSpawnSystem; itemSpawnSystem = nullptr;
                    delete spectatorSystem; spectatorSystem = nullptr;
                    delete jumpscareSystem; jumpscareSystem = nullptr;
                    delete sanitySystem; sanitySystem = nullptr;
                    delete staminaSystem; staminaSystem = nullptr;
                    delete fogSystem; fogSystem = nullptr;
                    delete flashlight; flashlight = nullptr;
                    if (audioManager) { audioManager->shutdown(); delete audioManager; audioManager = nullptr; }
                    if (renderer) { renderer->shutdown(); delete renderer; renderer = nullptr; }
                    break;
                }

                try {
                    // Update network
                    client->update(deltaTime);

                    // Get local player
                    uint32_t localId = client->getLocalPlayerId();
                    Player* localPlayer = game->getPlayer(localId);

                    if (localPlayer) {
                        PlayerState& state = localPlayer->getStateMut();

                        // Handle Android touch input
                        handleAndroidPlayerInput(localPlayer, deltaTime);

                        // Also handle keyboard input (for desktop testing)
                        if (IsKeyDown(KEY_W)) localPlayer->moveForward(deltaTime);
                        if (IsKeyDown(KEY_S)) localPlayer->moveBackward(deltaTime);
                        if (IsKeyDown(KEY_A)) localPlayer->moveLeft(deltaTime);
                        if (IsKeyDown(KEY_D)) localPlayer->moveRight(deltaTime);

                        // Update systems (with null checks)
                        if (staminaSystem) staminaSystem->update(deltaTime, state);
                        if (sanitySystem) sanitySystem->update(deltaTime, state, !state.flashlightOn, 100.0f);

                        if (flashlight) {
                            Vector3 forward = computeForwardVector(state.rotation);
                            flashlight->update(deltaTime, state.position, forward, state.battery);
                        }

                        if (audioManager) {
                            audioManager->updateGhostProximity(100.0f);
                            audioManager->updateFootstepSounds(deltaTime, state);
                        }

                        if (jumpscareSystem) {
                            if (jumpscareSystem->checkDynamicTrigger(state, deltaTime, false, state.sanity)) {
                                jumpscareSystem->triggerJumpscare(localId, state.position);
                            }
                            jumpscareSystem->update(deltaTime);
                        }

                        if (fogSystem) fogSystem->update(deltaTime);

                        client->sendPlayerMove(state);
                    }

                    // Spectator mode for dead players
                    if (localPlayer && localPlayer->getState().isDead && spectatorSystem) {
                        std::vector<PlayerState> alivePlayers;
                        const auto& allPlayers = game->getPlayers();
                        for (const auto& [pid, player] : allPlayers) {
                            if (!player->getState().isDead && !player->getState().isSpectator) {
                                alivePlayers.push_back(player->getState());
                            }
                        }
                        spectatorSystem->update(deltaTime, alivePlayers);
                    }

                    // Render
                    renderer->beginFrame();
                    if (localPlayer) {
                        renderer->renderGame(*game, localPlayer->getState());
                        renderer->renderHUD(localPlayer->getState());
                        renderer->renderSanityEffects(localPlayer->getState().sanity);
                    }

                    if (localPlayer && jumpscareSystem) {
                        const PlayerState& ps = localPlayer->getState();
                        Camera3D camera = { 0 };
                        camera.position = ps.position;
                        camera.position.y += Kaikai::PLAYER_HEIGHT * 0.9f;
                        Vector3 fwd = computeForwardVector(ps.rotation);
                        camera.target = Vector3Add(camera.position, fwd);
                        camera.up = { 0.0f, 1.0f, 0.0f };
                        camera.fovy = 60.0f;
                        camera.projection = CAMERA_PERSPECTIVE;
                        jumpscareSystem->render(camera);
                    }

                    renderer->endFrame();

                    // Draw Android touch controls overlay
                    drawAndroidControls();

                    // Update audio
                    if (audioManager) audioManager->update(deltaTime);

                } catch (...) {
                    LOG_ERROR("Exception during game loop! Returning to menu.");
                    // Safe cleanup and return to menu
                    if (client) { client->disconnect(); delete client; client = nullptr; }
                    delete game; game = nullptr;
                    delete itemSpawnSystem; itemSpawnSystem = nullptr;
                    delete spectatorSystem; spectatorSystem = nullptr;
                    delete jumpscareSystem; jumpscareSystem = nullptr;
                    delete sanitySystem; sanitySystem = nullptr;
                    delete staminaSystem; staminaSystem = nullptr;
                    delete fogSystem; fogSystem = nullptr;
                    delete flashlight; flashlight = nullptr;
                    if (audioManager) { audioManager->shutdown(); delete audioManager; audioManager = nullptr; }
                    if (renderer) { renderer->shutdown(); delete renderer; renderer = nullptr; }

                    appState = AppState::CONNECTION_FAILED;
                    menuTime = 0.0f;
                }
                break;
            }

            // ---- CONNECTION FAILED ----
            case AppState::CONNECTION_FAILED: {
                menuTime += deltaTime;

                drawConnectionFailedScreen(screenWidth, screenHeight, menuTime,
                                           serverIP.c_str());

                if (tapDetected || IsKeyPressed(KEY_ENTER)) {
                    appState = AppState::CONNECTING;
                    menuTime = 0.0f;
                }

                if (IsKeyPressed(KEY_ESCAPE)) {
                    CloseWindow();
                    return 0;
                }
                break;
            }
        }
    }

    // Cleanup (with null safety)
    if (client) { client->disconnect(); delete client; }
    if (audioManager) { audioManager->shutdown(); delete audioManager; }
    if (renderer) { renderer->shutdown(); delete renderer; }
    delete game;
    delete flashlight;
    delete fogSystem;
    delete staminaSystem;
    delete sanitySystem;
    delete jumpscareSystem;
    delete spectatorSystem;
    delete itemSpawnSystem;

#else // !__ANDROID__
    // ========================================================================
    // DESKTOP CLIENT: Direct connection flow (same as original)
    // ========================================================================

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
            sanitySystem.update(deltaTime, state, !state.flashlightOn, 100.0f);

            Vector3 forward = computeForwardVector(state.rotation);
            flashlight.update(deltaTime, state.position, forward, state.battery);

            audioManager.updateGhostProximity(100.0f);
            audioManager.updateFootstepSounds(deltaTime, state);

            if (jumpscareSystem.checkDynamicTrigger(state, deltaTime, false, state.sanity)) {
                jumpscareSystem.triggerJumpscare(localId, state.position);
            }
            jumpscareSystem.update(deltaTime);

            fogSystem.update(deltaTime);

            client.sendPlayerMove(state);
        }

        // Spectator mode for dead players
        if (localPlayer && localPlayer->getState().isDead) {
            std::vector<PlayerState> alivePlayers;
            const auto& allPlayers = game.getPlayers();
            for (const auto& [pid, player] : allPlayers) {
                if (!player->getState().isDead && !player->getState().isSpectator) {
                    alivePlayers.push_back(player->getState());
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

        if (localPlayer) {
            const PlayerState& ps = localPlayer->getState();
            Camera3D camera = { 0 };
            camera.position = ps.position;
            camera.position.y += Kaikai::PLAYER_HEIGHT * 0.9f;
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
#endif  // !__ANDROID__
#endif  // !KAIKAI_HEADLESS

    return 0;
}
