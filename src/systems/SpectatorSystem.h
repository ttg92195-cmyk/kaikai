#pragma once
#include "raylib.h"
#include <cstdint>
#include <vector>

struct SpectatorAction {
    enum Type {
        FLICK_LIGHT,   // toggle a nearby light
        DROP_OBJECT,   // make noise by dropping/breaking something
        WHISPER,       // create a whisper sound near a survivor
        POINT          // point toward an objective (visual indicator)
    };
    Type type;
    Vector3 position;
    float cooldown;
};

class SpectatorSystem {
public:
    SpectatorSystem() = default;
    void update(float deltaTime, const std::vector<PlayerState>& alivePlayers);
    
    // Spectator actions (dead players can perform these)
    void performAction(uint32_t spectatorId, SpectatorAction::Type type);
    
    // Rendering
    void renderSpectatorView(const Camera3D& camera, const PlayerState& spectator) const;
    void renderSpectatorUI() const;
    
    // Available actions for current spectator
    const std::vector<SpectatorAction>& getAvailableActions() const;
    
    // Check if a spectator can interact with something at position
    bool canInteract(Vector3 spectatorPos, Vector3 targetPos) const;
    
private:
    std::vector<SpectatorAction> availableActions;
    std::vector<uint32_t> spectators; // IDs of dead players
    float actionCooldown = 0.0f;
    static constexpr float ACTION_COOLDOWN_TIME = 5.0f;
    static constexpr float INTERACTION_RANGE = 8.0f;
    
    void updateAvailableActions(Vector3 spectatorPos, const std::vector<PlayerState>& alivePlayers);
};
