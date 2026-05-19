#include "NetworkManager.h"
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

NetworkManager::NetworkManager()  = default;

NetworkManager::~NetworkManager()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// ENet global init / shutdown
// ---------------------------------------------------------------------------

bool NetworkManager::init()
{
    if (enetInitialised) return true;

    if (enet_initialize() != 0) {
        fprintf(stderr, "[NetworkManager] ENet failed to initialise.\n");
        return false;
    }
    enetInitialised = true;
    printf("[NetworkManager] ENet initialised successfully.\n");
    return true;
}

void NetworkManager::shutdown()
{
    if (host) {
        // Gracefully disconnect every peer (0 ms timeout — fire-and-forget;
        // a production build may want to wait briefly).
        for (auto& [id, peer] : peers) {
            enet_peer_disconnect_now(peer, 0);
        }
        peers.clear();
        enet_host_destroy(host);
        host = nullptr;
    }

    if (enetInitialised) {
        enet_deinitialize();
        enetInitialised = false;
    }
}

// ---------------------------------------------------------------------------
// Event polling
// ---------------------------------------------------------------------------

void NetworkManager::pollEvents()
{
    if (!host) return;

    ENetEvent event;
    while (enet_host_service(host, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            uint32_t peerId = nextPeerId++;
            event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(peerId));
            peers[peerId] = event.peer;
            printf("[NetworkManager] Peer connected (id=%u, ip=%u:%u).\n",
                   peerId,
                   event.peer->address.host,
                   event.peer->address.port);
            if (connectCb) connectCb(peerId);
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
            uint32_t peerId = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
            peers.erase(peerId);
            printf("[NetworkManager] Peer disconnected (id=%u).\n", peerId);
            if (disconnectCb) disconnectCb(peerId);
            event.peer->data = nullptr;
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
            if (event.packet->dataLength >= sizeof(PacketHeader)) {
                const auto* header = reinterpret_cast<const PacketHeader*>(event.packet->data);
                if (packetCb) {
                    packetCb(*header, event.packet->data, event.packet->dataLength);
                }
            }
            enet_packet_destroy(event.packet);
            break;
        }

        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Sending helpers
// ---------------------------------------------------------------------------

bool NetworkManager::sendPacket(ENetPeer* peer, const void* data, size_t size, bool reliable)
{
    if (!peer || !data || size == 0) return false;

    enet_uint32 flags = reliable
        ? ENET_PACKET_FLAG_RELIABLE
        : ENET_PACKET_FLAG_UNSEQUENCED;

    uint8_t channel = reliable ? CHANNEL_RELIABLE : CHANNEL_UNRELIABLE;

    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (!packet) {
        fprintf(stderr, "[NetworkManager] Failed to create packet.\n");
        return false;
    }

    if (enet_peer_send(peer, channel, packet) < 0) {
        enet_packet_destroy(packet);
        fprintf(stderr, "[NetworkManager] Failed to send packet to peer.\n");
        return false;
    }

    // Flush immediately so the packet goes out this frame
    enet_host_flush(host);
    return true;
}

bool NetworkManager::broadcastPacket(const void* data, size_t size, bool reliable)
{
    if (!host || !data || size == 0) return false;

    enet_uint32 flags = reliable
        ? ENET_PACKET_FLAG_RELIABLE
        : ENET_PACKET_FLAG_UNSEQUENCED;

    uint8_t channel = reliable ? CHANNEL_RELIABLE : CHANNEL_UNRELIABLE;

    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (!packet) {
        fprintf(stderr, "[NetworkManager] Failed to create broadcast packet.\n");
        return false;
    }

    enet_host_broadcast(host, channel, packet);
    enet_host_flush(host);
    return true;
}

// ---------------------------------------------------------------------------
// Callback registration
// ---------------------------------------------------------------------------

void NetworkManager::onConnect(ConnectCallback cb)       { connectCb    = std::move(cb); }
void NetworkManager::onDisconnect(DisconnectCallback cb)  { disconnectCb = std::move(cb); }
void NetworkManager::onPacket(PacketCallback cb)          { packetCb     = std::move(cb); }

// ---------------------------------------------------------------------------
// Peer helpers
// ---------------------------------------------------------------------------

ENetPeer* NetworkManager::getPeer(uint32_t peerId) const
{
    auto it = peers.find(peerId);
    return it != peers.end() ? it->second : nullptr;
}

size_t NetworkManager::getPeerCount() const
{
    return peers.size();
}
