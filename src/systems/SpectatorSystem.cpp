#include "SpectatorSystem.h"
#include "../utils/Constants.h"
#include <algorithm>
#include <cmath>

void SpectatorSystem::update(float deltaTime, const std::vector<PlayerState>& alivePlayers) {
    // Update action cooldown
    if (actionCooldown > 0.0f) {
        actionCooldown -= deltaTime;
        if (actionCooldown < 0.0f) {
            actionCooldown = 0.0f;
        }
    }

    // Update cooldowns on individual available actions
    for (auto& action : availableActions) {
        if (action.cooldown > 0.0f) {
            action.cooldown -= deltaTime;
            if (action.cooldown < 0.0f) {
                action.cooldown = 0.0f;
            }
        }
    }

    // Refresh the list of spectators from alive players list
    // Players not in the alive list are considered dead/spectating
    spectators.clear();
    // (In a full implementation, the game would provide the full player list
    // and we'd identify dead ones. For now we rely on external registration.)

    // Update available actions for the current spectator position
    // If there are alive players, generate actions near them
    if (!alivePlayers.empty()) {
        // Use the first alive player's position as a reference for action generation
        Vector3 refPos = alivePlayers[0].position;
        updateAvailableActions(refPos, alivePlayers);
    }

    // Remove expired actions (cooldown fully elapsed and not reusable)
    availableActions.erase(
        std::remove_if(availableActions.begin(), availableActions.end(),
            [](const SpectatorAction& a) { return a.cooldown < -10.0f; }),
        availableActions.end()
    );
}

void SpectatorSystem::performAction(uint32_t spectatorId, SpectatorAction::Type type) {
    // Check global action cooldown
    if (actionCooldown > 0.0f) {
        return; // Still on cooldown, action fails
    }

    // Find an available action of the requested type
    for (auto& action : availableActions) {
        if (action.type == type && action.cooldown <= 0.0f) {
            // Execute the action based on type
            switch (type) {
                case SpectatorAction::FLICK_LIGHT:
                    // Toggle the nearest light source at the action position
                    // This would interface with a lighting system in the full game.
                    // For now, we mark the action as used and set its cooldown.
                    break;

                case SpectatorAction::DROP_OBJECT:
                    // Create a sound event at the action position.
                    // This would trigger an audio event that nearby survivors can hear.
                    // The sound draws the ghost's attention or alerts survivors.
                    break;

                case SpectatorAction::WHISPER:
                    // Play a whisper sound near the closest alive survivor.
                    // This is a subtle audio cue that can guide or warn survivors.
                    break;

                case SpectatorAction::POINT:
                    // Show a visual indicator pointing toward the nearest
                    // key item or exit door. This appears as a faint glowing
                    // arrow or beam of light visible to nearby survivors.
                    break;
            }

            // Set cooldown on the action
            action.cooldown = ACTION_COOLDOWN_TIME;
            actionCooldown = ACTION_COOLDOWN_TIME;
            return;
        }
    }
}

#if !defined(KAIKAI_HEADLESS)
void SpectatorSystem::renderSpectatorView(const Camera3D& camera, const PlayerState& spectator) const {
    // The entire world is rendered with a blue spectral tint for spectators
    Color blueTint = { 30, 60, 120, 40 };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), blueTint);

    // === Dead player spirit rendering ===
    // Dead players appear as translucent blue spirits
    // The spectator sees themselves as a faint blue outline
    Color spiritColor = { 80, 140, 220, 120 };
    DrawSphere(spectator.position, 0.4f, spiritColor);

    // Spirit "aura" particles
    float time = (float)GetTime();
    for (int i = 0; i < 6; i++) {
        float angle = time * 1.5f + i * 1.047f; // 6 particles evenly distributed
        float radius = 0.6f + sinf(time * 2.0f + i) * 0.1f;
        Vector3 particlePos = spectator.position;
        particlePos.x += cosf(angle) * radius;
        particlePos.z += sinf(angle) * radius;
        particlePos.y += sinf(time * 3.0f + i * 0.5f) * 0.3f;

        Color particleColor = { 100, 160, 240, (unsigned char)(80 + sinf(time * 4.0f + i) * 40) };
        DrawSphere(particlePos, 0.05f, particleColor);
    }

    // === Wall transparency effect ===
    // Spectators can see through walls slightly
    // This is achieved by rendering a semi-transparent overlay that
    // gives the impression of wall transparency
    Color wallXray = { 20, 50, 100, 15 };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), wallXray);

    // === Free camera indicator ===
    // Visual indicator that the spectator is in free-camera mode
    // Draw a subtle crosshair at screen center
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    Color crosshairColor = { 100, 180, 255, 100 };
    DrawLine(cx - 10, cy, cx + 10, cy, crosshairColor);
    DrawLine(cx, cy - 10, cx, cy + 10, crosshairColor);
}
#endif // !KAIKAI_HEADLESS

#if !defined(KAIKAI_HEADLESS)
void SpectatorSystem::renderSpectatorUI() const {
    int screenHeight = GetScreenHeight();

    // === "You are dead" message ===
    const char* deathMessage = "YOU ARE DEAD";
    int fontSize = 48;
    int textWidth = MeasureText(deathMessage, fontSize);
    Color deathColor = { 200, 50, 50, 200 };
    DrawText(deathMessage, (screenWidth - textWidth) / 2, screenHeight / 6, fontSize, deathColor);

    // === Subtitle ===
    const char* subtitle = "You are now a spectator. Help the survivors from beyond.";
    int subFontSize = 18;
    int subWidth = MeasureText(subtitle, subFontSize);
    Color subColor = { 150, 180, 220, 180 };
    DrawText(subtitle, (screenWidth - subWidth) / 2, screenHeight / 6 + 60, subFontSize, subColor);

    // === Available actions ===
    int actionY = screenHeight - 180;
    const char* actionsTitle = "Spectator Actions:";
    DrawText(actionsTitle, 20, actionY, 20, { 180, 200, 240, 220 });
    actionY += 30;

    const char* actionNames[] = { "FLICK LIGHT", "DROP OBJECT", "WHISPER", "POINT" };
    const char* actionKeys[] = { "[1]", "[2]", "[3]", "[4]" };

    for (size_t i = 0; i < availableActions.size() && i < 4; i++) {
        const auto& action = availableActions[i];
        bool onCooldown = action.cooldown > 0.0f;

        Color actionColor;
        if (onCooldown) {
            actionColor = { 100, 100, 100, 150 };
        } else {
            actionColor = { 140, 200, 255, 230 };
        }

        // Draw action key and name
        DrawText(actionKeys[i], 30, actionY, 16, actionColor);
        DrawText(actionNames[static_cast<int>(action.type)], 65, actionY, 16, actionColor);

        // Draw cooldown timer if applicable
        if (onCooldown) {
            const char* cdText = TextFormat("%.1fs", action.cooldown);
            DrawText(cdText, 220, actionY, 16, { 255, 150, 50, 200 });
        }

        actionY += 25;
    }

    // === Global cooldown indicator ===
    if (actionCooldown > 0.0f) {
        const char* globalCd = TextFormat("Action cooldown: %.1f", actionCooldown);
        DrawText(globalCd, 20, actionY + 10, 16, { 255, 180, 80, 200 });
    }

    // === List of alive players ===
    int playerListX = screenWidth - 220;
    int playerListY = screenHeight - 120;
    DrawText("Alive Survivors:", playerListX, playerListY, 18, { 80, 220, 80, 220 });
    playerListY += 25;

    // Note: In a full implementation, we'd iterate over the actual alive players
    // For now, render a placeholder message
    DrawText("(See HUD for player list)", playerListX, playerListY, 14, { 150, 150, 150, 180 });

    // === Free camera instructions ===
    const char* camInstructions = "WASD: Move | Mouse: Look | Shift: Fly Up | Ctrl: Fly Down";
    int instrWidth = MeasureText(camInstructions, 14);
    DrawText(camInstructions, (screenWidth - instrWidth) / 2, screenHeight - 30, 14,
             { 150, 180, 220, 160 });
}
#endif // !KAIKAI_HEADLESS

const std::vector<SpectatorAction>& SpectatorSystem::getAvailableActions() const {
    return availableActions;
}

bool SpectatorSystem::canInteract(Vector3 spectatorPos, Vector3 targetPos) const {
    float dx = spectatorPos.x - targetPos.x;
    float dy = spectatorPos.y - targetPos.y;
    float dz = spectatorPos.z - targetPos.z;
    float distSq = dx * dx + dy * dy + dz * dz;
    float rangeSq = INTERACTION_RANGE * INTERACTION_RANGE;
    return distSq <= rangeSq;
}

void SpectatorSystem::updateAvailableActions(Vector3 spectatorPos, const std::vector<PlayerState>& alivePlayers) {
    // Generate available actions based on nearby interactable objects
    // and alive player positions

    // Clear old fully-expired actions
    availableActions.erase(
        std::remove_if(availableActions.begin(), availableActions.end(),
            [](const SpectatorAction& a) { return a.cooldown <= 0.0f; }),
        availableActions.end()
    );

    // Only generate new actions if we have capacity (max 4 action types)
    bool hasFlickLight = false;
    bool hasDropObject = false;
    bool hasWhisper = false;
    bool hasPoint = false;

    for (const auto& action : availableActions) {
        switch (action.type) {
            case SpectatorAction::FLICK_LIGHT:  hasFlickLight = true; break;
            case SpectatorAction::DROP_OBJECT:  hasDropObject = true; break;
            case SpectatorAction::WHISPER:      hasWhisper = true; break;
            case SpectatorAction::POINT:        hasPoint = true; break;
        }
    }

    // Generate FLICK_LIGHT action near the spectator position
    // (In full game, this would be near actual light sources)
    if (!hasFlickLight) {
        SpectatorAction flickAction;
        flickAction.type = SpectatorAction::FLICK_LIGHT;
        flickAction.position = spectatorPos;
        flickAction.cooldown = 0.0f;
        availableActions.push_back(flickAction);
    }

    // Generate DROP_OBJECT action near the spectator position
    if (!hasDropObject) {
        SpectatorAction dropAction;
        dropAction.type = SpectatorAction::DROP_OBJECT;
        dropAction.position = spectatorPos;
        dropAction.cooldown = 0.0f;
        availableActions.push_back(dropAction);
    }

    // Generate WHISPER action near the closest alive player
    if (!hasWhisper && !alivePlayers.empty()) {
        // Find closest alive player
        float closestDist = 999999.0f;
        Vector3 closestPos = spectatorPos;
        for (const auto& player : alivePlayers) {
            float dx = player.position.x - spectatorPos.x;
            float dy = player.position.y - spectatorPos.y;
            float dz = player.position.z - spectatorPos.z;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist < closestDist) {
                closestDist = dist;
                closestPos = player.position;
            }
        }

        // Only offer whisper if within interaction range
        if (canInteract(spectatorPos, closestPos)) {
            SpectatorAction whisperAction;
            whisperAction.type = SpectatorAction::WHISPER;
            whisperAction.position = closestPos;
            whisperAction.cooldown = 0.0f;
            availableActions.push_back(whisperAction);
        }
    }

    // Generate POINT action (always available, points toward nearest objective)
    if (!hasPoint) {
        SpectatorAction pointAction;
        pointAction.type = SpectatorAction::POINT;
        pointAction.position = spectatorPos;
        pointAction.cooldown = 0.0f;
        availableActions.push_back(pointAction);
    }
}
