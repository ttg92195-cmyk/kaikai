#pragma once
#include "NetworkManager.h"
#include "../game/Game.h"

class Server : public NetworkManager {
public:
    Server();
    ~Server() = default;

    bool start(int port = SERVER_PORT);
    void stop();
    void update(float deltaTime);   // called each game tick

    // Server-authoritative game-state broadcast
    void broadcastGameState();

    // Per-packet handlers
    void handlePlayerMove(uint32_t playerId, const PlayerMovePacket& pkt);
    void handlePlayerAction(uint32_t playerId, const PlayerActionPacket& pkt);
    void handleVoiceData(uint32_t playerId, const VoiceDataPacket& pkt);
    void handleItemPickup(uint32_t playerId, const ItemPickupPacket& pkt);

    Game& getGame();

private:
    Game  game;
    float updateTimer         = 0.0f;
    float stateBroadcastTimer = 0.0f;

    // Maps ENet peer-id -> game PlayerId
    std::unordered_map<uint32_t, uint32_t> peerToPlayer;
    std::unordered_map<uint32_t, uint32_t> playerToPeer;

    // Timestamp origin (for packet timestamps)
    uint32_t serverStartTime = 0;

    uint32_t getTimestamp() const;

    void processPacket(const PacketHeader& header,
                       const uint8_t* data,
                       size_t size,
                       uint32_t senderId);

    // Called when a new client connects
    void onClientConnect(uint32_t peerId);
    // Called when a client disconnects
    void onClientDisconnect(uint32_t peerId);

    // Map a game-player id back to an ENet peer (for targeted sends)
    ENetPeer* peerForPlayer(uint32_t playerId) const;
};
