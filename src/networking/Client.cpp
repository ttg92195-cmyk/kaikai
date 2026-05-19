#include "Client.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Client::Client()
{
    onConnect([this](uint32_t peerId) {
        // For a client, the only peer is the server.
        printf("[Client] Connected to server (peer %u).\n", peerId);
        connected = true;
    });

    onDisconnect([this](uint32_t peerId) {
        connected    = false;
        serverPeer   = nullptr;
        localPlayerId = 0;
        remotePlayers.clear();
        interpolated.clear();
        printf("[Client] Disconnected from server.\n");
    });

    onPacket([this](const PacketHeader& header, const uint8_t* data, size_t size) {
        processPacket(header, data, size);
    });
}

// ---------------------------------------------------------------------------
// Connect / disconnect
// ---------------------------------------------------------------------------

bool Client::connect(const char* hostAddr, int port)
{
    if (!init()) return false;

    // Client host: 1 outgoing connection, 3 channels
    host = enet_host_create(nullptr, 1, 3, 0, 0);
    if (!host) {
        fprintf(stderr, "[Client] Failed to create ENet client host.\n");
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, hostAddr);
    address.port = static_cast<enet_uint16>(port);

    serverPeer = enet_host_connect(host, &address, 3, 0);
    if (!serverPeer) {
        fprintf(stderr, "[Client] Failed to initiate connection to %s:%d.\n", hostAddr, port);
        enet_host_destroy(host);
        host = nullptr;
        return false;
    }

    // Wait up to 5 seconds for the connection to succeed
    ENetEvent event;
    if (enet_host_service(host, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        serverPeer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(1));
        peers[1] = serverPeer;
        connected = true;
        printf("[Client] Connected to %s:%d.\n", hostAddr, port);
        return true;
    }

    // Connection failed or timed out
    enet_peer_reset(serverPeer);
    serverPeer = nullptr;
    enet_host_destroy(host);
    host = nullptr;
    fprintf(stderr, "[Client] Connection to %s:%d timed out.\n", hostAddr, port);
    return false;
}

void Client::disconnect()
{
    if (serverPeer) {
        // Attempt graceful disconnect (wait up to 3 s)
        enet_peer_disconnect(serverPeer, 0);

        ENetEvent event;
        while (enet_host_service(host, &event, 3000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_DISCONNECT:
                printf("[Client] Gracefully disconnected from server.\n");
                goto done;
            case ENET_EVENT_TYPE_RECEIVE:
                enet_packet_destroy(event.packet);
                break;
            default:
                break;
            }
        }

        // Force reset if graceful disconnect didn't complete
        enet_peer_reset(serverPeer);
    done:
        serverPeer    = nullptr;
        connected     = false;
        localPlayerId = 0;
    }

    shutdown();
    remotePlayers.clear();
    interpolated.clear();
}

// ---------------------------------------------------------------------------
// Main update loop
// ---------------------------------------------------------------------------

void Client::update(float deltaTime)
{
    if (!connected) return;

    pollEvents();
    interpolatePlayers(deltaTime);
    game.update(deltaTime);
}

// ---------------------------------------------------------------------------
// Send: player movement
// ---------------------------------------------------------------------------

void Client::sendPlayerMove(const PlayerState& state)
{
    if (!serverPeer || !connected) return;

    PlayerMovePacket pkt{};
    pkt.header.type      = PacketType::PLAYER_MOVE;
    pkt.header.senderId  = localPlayerId;
    pkt.header.timestamp = static_cast<uint32_t>(GetTime() * 1000.0);
    pkt.position         = state.position;
    pkt.rotation         = state.rotation;
    pkt.isRunning        = (state.stamina < 100.0f && state.stamina > 0.0f);
    pkt.flashlightOn     = state.flashlightOn;

    sendPacket(serverPeer, &pkt, sizeof(pkt), false);
}

// ---------------------------------------------------------------------------
// Send: player action
// ---------------------------------------------------------------------------

void Client::sendPlayerAction(PlayerActionPacket::Action action, uint32_t targetId)
{
    if (!serverPeer || !connected) return;

    PlayerActionPacket pkt{};
    pkt.header.type      = PacketType::PLAYER_ACTION;
    pkt.header.senderId  = localPlayerId;
    pkt.header.timestamp = static_cast<uint32_t>(GetTime() * 1000.0);
    pkt.action           = action;
    pkt.targetId         = targetId;

    sendPacket(serverPeer, &pkt, sizeof(pkt), true);
}

// ---------------------------------------------------------------------------
// Send: voice data
// ---------------------------------------------------------------------------

void Client::sendVoiceData(const uint8_t* data, size_t length)
{
    if (!serverPeer || !connected || !data || length == 0) return;

    VoiceDataPacket pkt{};
    pkt.header.type      = PacketType::VOICE_DATA;
    pkt.header.senderId  = localPlayerId;
    pkt.header.timestamp = static_cast<uint32_t>(GetTime() * 1000.0);

    size_t copyLen = std::min(length, sizeof(pkt.data));
    memcpy(pkt.data, data, copyLen);
    pkt.dataLength = static_cast<uint32_t>(copyLen);
    pkt.volume     = 1.0f;

    // Voice goes on the unreliable channel for low latency
    sendPacket(serverPeer, &pkt, sizeof(pkt), false);
}

// ---------------------------------------------------------------------------
// Send: chat message
// ---------------------------------------------------------------------------

void Client::sendChatMessage(const char* message)
{
    if (!serverPeer || !connected || !message) return;

    ChatMessagePacket pkt{};
    pkt.header.type      = PacketType::CHAT_MESSAGE;
    pkt.header.senderId  = localPlayerId;
    pkt.header.timestamp = static_cast<uint32_t>(GetTime() * 1000.0);
    strncpy(pkt.message, message, sizeof(pkt.message) - 1);
    pkt.message[sizeof(pkt.message) - 1] = '\0';

    sendPacket(serverPeer, &pkt, sizeof(pkt), true);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const std::vector<PlayerState>& Client::getRemotePlayers() const { return remotePlayers; }
uint32_t  Client::getLocalPlayerId() const { return localPlayerId; }
bool      Client::isConnected()      const { return connected; }
GameState Client::getGameState()     const { return gameState; }

// ---------------------------------------------------------------------------
// Packet processing
// ---------------------------------------------------------------------------

void Client::processPacket(const PacketHeader& header, const uint8_t* data, size_t size)
{
    switch (header.type) {

    case PacketType::PLAYER_JOIN: {
        if (size < sizeof(PlayerJoinPacket)) break;
        auto pkt = *reinterpret_cast<const PlayerJoinPacket*>(data);

        // If this is the first JOIN we receive and we don't have an id yet,
        // the senderId is *our* assigned player id.
        if (localPlayerId == 0) {
            localPlayerId = pkt.header.senderId;
            printf("[Client] Assigned player id %u.\n", localPlayerId);
        }

        // Add the new remote player (skip ourselves)
        if (pkt.header.senderId != localPlayerId) {
            PlayerState ps{};
            ps.id          = pkt.header.senderId;
            ps.position    = { 0, 0, 0 };
            ps.rotation    = 0.0f;
            ps.stamina     = 100.0f;
            ps.sanity      = 100.0f;
            ps.battery     = 100.0f;
            ps.isGhost     = false;
            ps.isDead      = false;
            ps.isSpectator = false;
            ps.flashlightOn = false;

            InterpolatedPlayer ip{};
            ip.previous = ps;
            ip.current  = ps;
            ip.alpha    = 1.0f;
            interpolated[ps.id] = ip;

            rebuildRemotePlayersList();
            printf("[Client] Player %u joined.\n", pkt.header.senderId);
        }
        break;
    }

    case PacketType::PLAYER_LEAVE: {
        if (size < sizeof(PlayerLeavePacket)) break;
        auto pkt = *reinterpret_cast<const PlayerLeavePacket*>(data);
        interpolated.erase(pkt.header.senderId);
        rebuildRemotePlayersList();
        printf("[Client] Player %u left.\n", pkt.header.senderId);
        break;
    }

    case PacketType::GAME_STATE: {
        if (size < sizeof(GameStatePacket)) break;
        auto pkt = *reinterpret_cast<const GameStatePacket*>(data);
        gameState = static_cast<GameState>(pkt.gameState);

        for (uint8_t i = 0; i < pkt.playerCount; ++i) {
            const PlayerState& remote = pkt.players[i];

            // Skip local player — we trust our own input
            if (remote.id == localPlayerId) continue;

            auto it = interpolated.find(remote.id);
            if (it != interpolated.end()) {
                // Shift current -> previous, new state -> current
                it->second.previous = it->second.current;
                it->second.current  = remote;
                it->second.alpha    = 0.0f;
            } else {
                // First time seeing this player — snap
                InterpolatedPlayer ip{};
                ip.previous = remote;
                ip.current  = remote;
                ip.alpha    = 1.0f;
                interpolated[remote.id] = ip;
            }
        }

        // Remove players that are no longer in the state snapshot
        std::unordered_map<uint32_t, bool> activeIds;
        for (uint8_t i = 0; i < pkt.playerCount; ++i) {
            activeIds[pkt.players[i].id] = true;
        }
        for (auto it = interpolated.begin(); it != interpolated.end(); ) {
            if (!activeIds.count(it->first)) {
                it = interpolated.erase(it);
            } else {
                ++it;
            }
        }

        rebuildRemotePlayersList();
        break;
    }

    case PacketType::CHAT_MESSAGE: {
        if (size < sizeof(ChatMessagePacket)) break;
        auto pkt = *reinterpret_cast<const ChatMessagePacket*>(data);
        printf("[Chat] Player %u: %s\n", pkt.header.senderId, pkt.message);
        break;
    }

    case PacketType::VOICE_DATA: {
        if (size < sizeof(VoiceDataPacket)) break;
        // In a full implementation, feed pkt.data / pkt.dataLength /
        // pkt.volume into the audio playback system.
        // Here we simply acknowledge receipt.
        break;
    }

    case PacketType::ITEM_PICKUP: {
        if (size < sizeof(ItemPickupPacket)) break;
        auto pkt = *reinterpret_cast<const ItemPickupPacket*>(data);
        printf("[Client] Player %u picked up item %u (type=%u).\n",
               pkt.header.senderId, pkt.itemId, pkt.itemType);
        break;
    }

    case PacketType::DOOR_TOGGLE: {
        if (size < sizeof(DoorTogglePacket)) break;
        auto pkt = *reinterpret_cast<const DoorTogglePacket*>(data);
        // Update client-side world state
        Door* door = game.getDoor(pkt.doorId);
        if (door) door->isOpen = pkt.open;
        break;
    }

    case PacketType::SWITCH_TOGGLE: {
        if (size < sizeof(SwitchTogglePacket)) break;
        auto pkt = *reinterpret_cast<const SwitchTogglePacket*>(data);
        Switch* sw = game.getSwitch(pkt.switchId);
        if (sw) sw->isPressed = pkt.pressed;
        break;
    }

    case PacketType::GHOST_CATCH: {
        if (size < sizeof(GhostCatchPacket)) break;
        auto pkt = *reinterpret_cast<const GhostCatchPacket*>(data);
        // Mark the victim as dead locally
        auto vit = interpolated.find(pkt.victimId);
        if (vit != interpolated.end()) {
            vit->second.current.isDead = true;
            vit->second.previous.isDead = true;
        }
        printf("[Client] Ghost caught player %u!\n", pkt.victimId);
        break;
    }

    case PacketType::SANITY_UPDATE: {
        if (size < sizeof(SanityUpdatePacket)) break;
        auto pkt = *reinterpret_cast<const SanityUpdatePacket*>(data);
        if (pkt.header.senderId == localPlayerId) {
            PlayerState* ps = game.getPlayerState(localPlayerId);
            if (ps) ps->sanity = pkt.sanity;
        }
        break;
    }

    case PacketType::BATTERY_UPDATE: {
        if (size < sizeof(BatteryUpdatePacket)) break;
        auto pkt = *reinterpret_cast<const BatteryUpdatePacket*>(data);
        if (pkt.header.senderId == localPlayerId) {
            PlayerState* ps = game.getPlayerState(localPlayerId);
            if (ps) ps->battery = pkt.battery;
        }
        break;
    }

    case PacketType::JUMPSCARE_TRIGGER: {
        if (size < sizeof(JumpscarePacket)) break;
        auto pkt = *reinterpret_cast<const JumpscarePacket*>(data);
        if (pkt.targetPlayerId == localPlayerId) {
            printf("[Client] JUMPSCARE! Duration %.2f s.\n", pkt.duration);
            // Trigger the client-side jump-scare renderer here.
        }
        break;
    }

    case PacketType::ITEM_SPAWN: {
        if (size < sizeof(ItemSpawnPacket)) break;
        auto pkt = *reinterpret_cast<const ItemSpawnPacket*>(data);
        // Register the spawned item in the client-side game world
        printf("[Client] Item %u spawned (type=%u) at (%.1f, %.1f, %.1f).\n",
               pkt.itemId, pkt.itemType,
               pkt.position.x, pkt.position.y, pkt.position.z);
        break;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Interpolation — smooth remote player movement between snapshots
// ---------------------------------------------------------------------------

void Client::interpolatePlayers(float deltaTime)
{
    // We lerp over the interval between state broadcasts.
    // With NETWORK_UPDATE_RATE = 20 Hz, that is 50 ms per snap.
    const float INTERP_PERIOD = STATE_BROADCAST_INTERVAL;

    for (auto& [id, ip] : interpolated) {
        if (ip.alpha >= 1.0f) continue;

        ip.alpha += deltaTime / INTERP_PERIOD;
        if (ip.alpha > 1.0f) ip.alpha = 1.0f;

        float t = ip.alpha;

        // Lerp position
        ip.previous.position.x = ip.previous.position.x +
            (ip.current.position.x - ip.previous.position.x) * t;
        ip.previous.position.y = ip.previous.position.y +
            (ip.current.position.y - ip.previous.position.y) * t;
        ip.previous.position.z = ip.previous.position.z +
            (ip.current.position.z - ip.previous.position.z) * t;

        // Lerp rotation (simple linear — fine for yaw)
        ip.previous.rotation = ip.previous.rotation +
            (ip.current.rotation - ip.previous.rotation) * t;

        // Lerp scalar resources for smooth HUD display
        ip.previous.stamina = ip.previous.stamina +
            (ip.current.stamina - ip.previous.stamina) * t;
        ip.previous.sanity = ip.previous.sanity +
            (ip.current.sanity - ip.previous.sanity) * t;
        ip.previous.battery = ip.previous.battery +
            (ip.current.battery - ip.previous.battery) * t;

        // Snap boolean fields
        ip.previous.isGhost      = ip.current.isGhost;
        ip.previous.isDead       = ip.current.isDead;
        ip.previous.isSpectator  = ip.current.isSpectator;
        ip.previous.flashlightOn = ip.current.flashlightOn;
    }

    rebuildRemotePlayersList();
}

// ---------------------------------------------------------------------------
// Rebuild the flat remote-players vector from interpolated states
// ---------------------------------------------------------------------------

void Client::rebuildRemotePlayersList()
{
    remotePlayers.clear();
    for (const auto& [id, ip] : interpolated) {
        remotePlayers.push_back(ip.previous);
    }
}
