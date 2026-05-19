#pragma once
#include <enet/enet.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include "PacketTypes.h"

class NetworkManager {
public:
    NetworkManager();
    virtual ~NetworkManager();

    // Initialise the ENet library (call once before any host creation)
    bool init();
    // Shut down ENet and destroy the host
    void shutdown();
    // Process incoming / outgoing ENet events — call once per frame
    void pollEvents();

    // ---------- Sending ----------
    // Send raw bytes to a specific peer.
    // reliable = true  -> ENET_PACKET_FLAG_RELIABLE on CHANNEL_RELIABLE
    // reliable = false -> ENET_PACKET_FLAG_UNSEQUENCED on CHANNEL_UNRELIABLE
    bool sendPacket(ENetPeer* peer, const void* data, size_t size, bool reliable = true);

    // Broadcast raw bytes to every connected peer.
    bool broadcastPacket(const void* data, size_t size, bool reliable = true);

    // ---------- Callbacks ----------
    using ConnectCallback    = std::function<void(uint32_t playerId)>;
    using DisconnectCallback = std::function<void(uint32_t playerId)>;
    using PacketCallback     = std::function<void(const PacketHeader& header,
                                                   const uint8_t* data,
                                                   size_t size)>;

    void onConnect(ConnectCallback cb);
    void onDisconnect(DisconnectCallback cb);
    void onPacket(PacketCallback cb);

    // ---------- Peer helpers ----------
    ENetPeer* getPeer(uint32_t peerId) const;
    size_t    getPeerCount() const;

protected:
    ENetHost*                                   host = nullptr;
    std::unordered_map<uint32_t, ENetPeer*>     peers;

private:
    ConnectCallback    connectCb;
    DisconnectCallback disconnectCb;
    PacketCallback     packetCb;
    uint32_t           nextPeerId = 1;
    bool               enetInitialised = false;
};
