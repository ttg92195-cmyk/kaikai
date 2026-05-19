#pragma once
#include "NetworkManager.h"
#include "../game/Game.h"

class Client : public NetworkManager {
public:
    Client();
    ~Client() = default;

    bool connect(const char* host, int port = SERVER_PORT);
    void disconnect();
    void update(float deltaTime);

    // ----- Send player input to server -----
    void sendPlayerMove(const PlayerState& state);
    void sendPlayerAction(PlayerActionPacket::Action action, uint32_t targetId = 0);
    void sendVoiceData(const uint8_t* data, size_t length);
    void sendChatMessage(const char* message);

    // ----- Accessors -----
    const std::vector<PlayerState>& getRemotePlayers() const;
    uint32_t  getLocalPlayerId() const;
    bool      isConnected() const;
    GameState getGameState() const;

private:
    ENetPeer*              serverPeer    = nullptr;
    uint32_t               localPlayerId = 0;
    std::vector<PlayerState> remotePlayers;
    Game                   game;          // client-side game for rendering
    GameState              gameState = GameState::LOBBY;
    bool                   connected = false;

    // Interpolation bookkeeping: for each remote player we store both the
    // "previous" and "current" snapshots received from the server so that we
    // can lerp between them at the rendering frame rate.
    struct InterpolatedPlayer {
        PlayerState previous;
        PlayerState current;
        float       alpha = 0.0f;   // 0..1 lerp factor
    };
    std::unordered_map<uint32_t, InterpolatedPlayer> interpolated;

    float moveSendTimer = 0.0f;
    float sanitySendTimer = 0.0f;
    float batterySendTimer = 0.0f;

    void processPacket(const PacketHeader& header, const uint8_t* data, size_t size);
    void interpolatePlayers(float deltaTime);
    void rebuildRemotePlayersList();
};
