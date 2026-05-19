#include "Server.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Server::Server()
{
    // Register NetworkManager callbacks that bridge into our per-packet logic
    onConnect([this](uint32_t peerId) {
        onClientConnect(peerId);
    });

    onDisconnect([this](uint32_t peerId) {
        onClientDisconnect(peerId);
    });

    onPacket([this](const PacketHeader& header, const uint8_t* data, size_t size) {
        // The senderId is filled in by the client with its assigned player id.
        // On the server we trust this as the authoritative player identifier
        // (the server assigned the id, so it can verify it later).
        processPacket(header, data, size, header.senderId);
    });

    serverStartTime = static_cast<uint32_t>(GetTime() * 1000.0);
}

// ---------------------------------------------------------------------------
// Timestamp helper
// ---------------------------------------------------------------------------

uint32_t Server::getTimestamp() const
{
    return static_cast<uint32_t>(GetTime() * 1000.0) - serverStartTime;
}

// ---------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------

bool Server::start(int port)
{
    if (!init()) return false;

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(port);

    // 3 channels: reliable, unreliable, voice
    host = enet_host_create(&address, MAX_PLAYERS, 3, 0, 0);
    if (!host) {
        fprintf(stderr, "[Server] Failed to create ENet server on port %d.\n", port);
        return false;
    }

    printf("[Server] Listening on port %d (max %d players).\n", port, MAX_PLAYERS);
    return true;
}

void Server::stop()
{
    // Notify all clients
    PlayerLeavePacket leave{};
    leave.header.type      = PacketType::PLAYER_LEAVE;
    leave.header.senderId  = 0;
    leave.header.timestamp = getTimestamp();
    broadcastPacket(&leave, sizeof(leave), true);

    shutdown();
    peerToPlayer.clear();
    playerToPeer.clear();
    printf("[Server] Stopped.\n");
}

// ---------------------------------------------------------------------------
// Main update loop
// ---------------------------------------------------------------------------

void Server::update(float deltaTime)
{
    pollEvents();

    // Tick game logic
    game.update(deltaTime);

    // Broadcast full game state at NETWORK_UPDATE_RATE
    stateBroadcastTimer += deltaTime;
    if (stateBroadcastTimer >= STATE_BROADCAST_INTERVAL) {
        stateBroadcastTimer -= STATE_BROADCAST_INTERVAL;
        broadcastGameState();
    }
}

// ---------------------------------------------------------------------------
// Client connect / disconnect
// ---------------------------------------------------------------------------

void Server::onClientConnect(uint32_t peerId)
{
    if (game.getAllNetworkPlayers().size() >= MAX_PLAYERS) {
        // Reject: server full — disconnect the peer immediately
        auto* peer = getPeer(peerId);
        if (peer) enet_peer_disconnect_now(peer, 0);
        printf("[Server] Rejected connection — server full (peer %u).\n", peerId);
        return;
    }

    uint32_t playerId = game.addPlayer("Player");
    if (playerId == 0) {
        auto* peer = getPeer(peerId);
        if (peer) enet_peer_disconnect_now(peer, 0);
        return;
    }

    peerToPlayer[peerId]   = playerId;
    playerToPeer[playerId] = peerId;

    // Send the new player their assigned id via a PLAYER_JOIN acknowledgement
    PlayerJoinPacket join{};
    join.header.type      = PacketType::PLAYER_JOIN;
    join.header.senderId  = playerId;   // this IS the assigned player id
    join.header.timestamp = getTimestamp();
    strncpy(join.playerName, "Player", sizeof(join.playerName) - 1);
    join.playerName[sizeof(join.playerName) - 1] = '\0';

    auto* peer = getPeer(peerId);
    if (peer) sendPacket(peer, &join, sizeof(join), true);

    // Broadcast join to everyone else
    broadcastPacket(&join, sizeof(join), true);

    printf("[Server] Player %u joined (peer %u).\n", playerId, peerId);
}

void Server::onClientDisconnect(uint32_t peerId)
{
    auto it = peerToPlayer.find(peerId);
    if (it == peerToPlayer.end()) return;

    uint32_t playerId = it->second;

    // Broadcast leave
    PlayerLeavePacket leave{};
    leave.header.type      = PacketType::PLAYER_LEAVE;
    leave.header.senderId  = playerId;
    leave.header.timestamp = getTimestamp();
    broadcastPacket(&leave, sizeof(leave), true);

    game.removePlayer(playerId);
    peerToPlayer.erase(peerId);
    playerToPeer.erase(playerId);

    printf("[Server] Player %u left (peer %u).\n", playerId, peerId);
}

// ---------------------------------------------------------------------------
// Peer lookup helper
// ---------------------------------------------------------------------------

ENetPeer* Server::peerForPlayer(uint32_t playerId) const
{
    auto it = playerToPeer.find(playerId);
    if (it == playerToPeer.end()) return nullptr;
    return getPeer(it->second);
}

// ---------------------------------------------------------------------------
// Packet dispatcher
// ---------------------------------------------------------------------------

void Server::processPacket(const PacketHeader& header,
                            const uint8_t* data,
                            size_t size,
                            uint32_t senderId)
{
    switch (header.type) {
    case PacketType::PLAYER_MOVE: {
        if (size >= sizeof(PlayerMovePacket)) {
            auto pkt = *reinterpret_cast<const PlayerMovePacket*>(data);
            handlePlayerMove(senderId, pkt);
        }
        break;
    }

    case PacketType::PLAYER_ACTION: {
        if (size >= sizeof(PlayerActionPacket)) {
            auto pkt = *reinterpret_cast<const PlayerActionPacket*>(data);
            handlePlayerAction(senderId, pkt);
        }
        break;
    }

    case PacketType::VOICE_DATA: {
        if (size >= sizeof(VoiceDataPacket)) {
            auto pkt = *reinterpret_cast<const VoiceDataPacket*>(data);
            handleVoiceData(senderId, pkt);
        }
        break;
    }

    case PacketType::ITEM_PICKUP: {
        if (size >= sizeof(ItemPickupPacket)) {
            auto pkt = *reinterpret_cast<const ItemPickupPacket*>(data);
            handleItemPickup(senderId, pkt);
        }
        break;
    }

    case PacketType::CHAT_MESSAGE: {
        if (size >= sizeof(ChatMessagePacket)) {
            // Rebroadcast chat to all players
            auto pkt = *reinterpret_cast<const ChatMessagePacket*>(data);
            pkt.header.senderId  = senderId;
            pkt.header.timestamp = getTimestamp();
            broadcastPacket(&pkt, sizeof(pkt), true);
        }
        break;
    }

    case PacketType::SANITY_UPDATE: {
        if (size >= sizeof(SanityUpdatePacket)) {
            auto pkt = *reinterpret_cast<const SanityUpdatePacket*>(data);
            game.updateSanity(senderId, pkt.sanity);
        }
        break;
    }

    case PacketType::BATTERY_UPDATE: {
        if (size >= sizeof(BatteryUpdatePacket)) {
            auto pkt = *reinterpret_cast<const BatteryUpdatePacket*>(data);
            game.updateBattery(senderId, pkt.battery);
        }
        break;
    }

    case PacketType::GHOST_CATCH: {
        if (size >= sizeof(GhostCatchPacket)) {
            auto pkt = *reinterpret_cast<const GhostCatchPacket*>(data);
            game.catchPlayer(senderId, pkt.victimId);

            // Broadcast catch event
            GhostCatchPacket broadcast{};
            broadcast.header.type      = PacketType::GHOST_CATCH;
            broadcast.header.senderId  = senderId;
            broadcast.header.timestamp = getTimestamp();
            broadcast.victimId         = pkt.victimId;
            broadcastPacket(&broadcast, sizeof(broadcast), true);
        }
        break;
    }

    default:
        // Ignore packet types the server doesn't need to process
        break;
    }
}

// ---------------------------------------------------------------------------
// Movement handler — basic anti-cheat validation
// ---------------------------------------------------------------------------

void Server::handlePlayerMove(uint32_t playerId, const PlayerMovePacket& pkt)
{
    PlayerState* ps = game.getPlayerState(playerId);
    if (!ps || ps->isDead || ps->isSpectator) return;

    // ----- Anti-cheat: distance check -----
    float dx   = pkt.position.x - ps->position.x;
    float dy   = pkt.position.y - ps->position.y;
    float dz   = pkt.position.z - ps->position.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    // We expect at most STATE_BROADCAST_INTERVAL worth of movement.
    // If the client claims to have moved further than possible, clamp.
    float maxDist = pkt.isRunning ? MAX_RUN_SPEED * STATE_BROADCAST_INTERVAL * 2.0f
                                  : MAX_MOVE_SPEED  * STATE_BROADCAST_INTERVAL * 2.0f;

    Vector3 newPos = pkt.position;
    if (dist > maxDist && dist > 0.0f) {
        // Clamp to the maximum allowed distance in the same direction
        float scale = maxDist / dist;
        newPos.x = ps->position.x + dx * scale;
        newPos.y = ps->position.y + dy * scale;
        newPos.z = ps->position.z + dz * scale;
        printf("[Server] Movement clamped for player %u (claimed %.2f, max %.2f).\n",
               playerId, dist, maxDist);
    }

    ps->position     = newPos;
    ps->rotation     = pkt.rotation;
    ps->flashlightOn = pkt.flashlightOn;

    // Drain stamina if running
    if (pkt.isRunning && ps->stamina > 0.0f) {
        ps->stamina -= 10.0f * STATE_BROADCAST_INTERVAL;
        if (ps->stamina < 0.0f) ps->stamina = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// Action handler
// ---------------------------------------------------------------------------

void Server::handlePlayerAction(uint32_t playerId, const PlayerActionPacket& pkt)
{
    PlayerState* ps = game.getPlayerState(playerId);
    if (!ps || ps->isDead || ps->isSpectator) return;

    switch (pkt.action) {
    case PlayerActionPacket::INTERACT: {
        // Generic interaction — for now, just rebroadcast
        PlayerActionPacket out = pkt;
        out.header.senderId  = playerId;
        out.header.timestamp = getTimestamp();
        broadcastPacket(&out, sizeof(out), true);
        break;
    }

    case PlayerActionPacket::TOGGLE_FLASHLIGHT: {
        ps->flashlightOn = !ps->flashlightOn;
        // If battery is dead, cannot turn on
        if (ps->flashlightOn && ps->battery <= 0.0f) {
            ps->flashlightOn = false;
        }
        PlayerActionPacket out{};
        out.header.type      = PacketType::PLAYER_ACTION;
        out.header.senderId  = playerId;
        out.header.timestamp = getTimestamp();
        out.action           = PlayerActionPacket::TOGGLE_FLASHLIGHT;
        out.targetId         = 0;
        broadcastPacket(&out, sizeof(out), true);
        break;
    }

    case PlayerActionPacket::PICK_UP_ITEM: {
        ItemPickupPacket pickup{};
        pickup.header.type      = PacketType::ITEM_PICKUP;
        pickup.header.senderId  = playerId;
        pickup.header.timestamp = getTimestamp();
        pickup.itemId           = pkt.targetId;

        if (game.tryPickupItem(playerId, pkt.targetId)) {
            Item* item = game.getItem(pkt.targetId);
            if (item) {
                switch (item->type) {
                case Item::KEY:        pickup.itemType = 0; break;
                case Item::BATTERY:    pickup.itemType = 1; break;
                case Item::NOTE:       pickup.itemType = 2; break;
                case Item::HEALTH_PACK:pickup.itemType = 3; break;
                default:               pickup.itemType = 0; break;
                }
            }
            broadcastPacket(&pickup, sizeof(pickup), true);
        }
        break;
    }

    case PlayerActionPacket::PRESS_SWITCH:
    case PlayerActionPacket::RELEASE_SWITCH: {
        bool pressed = (pkt.action == PlayerActionPacket::PRESS_SWITCH);
        game.setSwitch(pkt.targetId, pressed);

        SwitchTogglePacket out{};
        out.header.type      = PacketType::SWITCH_TOGGLE;
        out.header.senderId  = playerId;
        out.header.timestamp = getTimestamp();
        out.switchId         = pkt.targetId;
        out.pressed          = pressed;
        broadcastPacket(&out, sizeof(out), true);
        break;
    }

    case PlayerActionPacket::USE_ITEM: {
        // Broadcast item use to all clients
        PlayerActionPacket out = pkt;
        out.header.senderId  = playerId;
        out.header.timestamp = getTimestamp();
        broadcastPacket(&out, sizeof(out), true);
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Proximity voice chat
// ---------------------------------------------------------------------------

void Server::handleVoiceData(uint32_t playerId, const VoiceDataPacket& pkt)
{
    const PlayerState* sender = game.getPlayerState(playerId);
    if (!sender) return;

    // Forward voice data only to players within VOICE_CHAT_MAX_DISTANCE.
    // Adjust the volume field based on distance for spatial audio.
    for (const auto& [id, ps] : game.getAllNetworkPlayers()) {
        if (id == playerId) continue;  // don't echo back to sender

        float dx   = ps.position.x - sender->position.x;
        float dy   = ps.position.y - sender->position.y;
        float dz   = ps.position.z - sender->position.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist > VOICE_CHAT_MAX_DISTANCE) continue;

        // Linear attenuation: 1.0 at distance 0, 0.0 at max distance
        float volume = 1.0f - (dist / VOICE_CHAT_MAX_DISTANCE);

        VoiceDataPacket fwd = pkt;
        fwd.header.type      = PacketType::VOICE_DATA;
        fwd.header.senderId  = playerId;
        fwd.header.timestamp = getTimestamp();
        fwd.volume           = volume;

        ENetPeer* peer = peerForPlayer(id);
        if (peer) {
            sendPacket(peer, &fwd, sizeof(fwd), false);
        }
    }
}

// ---------------------------------------------------------------------------
// Item pickup handler (authoritative)
// ---------------------------------------------------------------------------

void Server::handleItemPickup(uint32_t playerId, const ItemPickupPacket& pkt)
{
    if (game.tryPickupItem(playerId, pkt.itemId)) {
        Item* item = game.getItem(pkt.itemId);
        ItemPickupPacket out{};
        out.header.type      = PacketType::ITEM_PICKUP;
        out.header.senderId  = playerId;
        out.header.timestamp = getTimestamp();
        out.itemId           = pkt.itemId;
        if (item) {
            switch (item->type) {
            case Item::KEY:         out.itemType = 0; break;
            case Item::BATTERY:     out.itemType = 1; break;
            case Item::NOTE:        out.itemType = 2; break;
            case Item::HEALTH_PACK: out.itemType = 3; break;
            default:                out.itemType = 0; break;
            }
        }
        broadcastPacket(&out, sizeof(out), true);
    }
}

// ---------------------------------------------------------------------------
// Game state broadcast
// ---------------------------------------------------------------------------

void Server::broadcastGameState()
{
    GameStatePacket state{};
    state.header.type      = PacketType::GAME_STATE;
    state.header.senderId  = 0;  // from server
    state.header.timestamp = getTimestamp();
    state.gameState        = static_cast<uint8_t>(game.getState());

    const auto& netPlayers = game.getAllNetworkPlayers();
    state.playerCount = static_cast<uint8_t>(
        std::min<size_t>(netPlayers.size(), MAX_PLAYERS));

    uint8_t idx = 0;
    for (const auto& [id, ps] : netPlayers) {
        if (idx >= MAX_PLAYERS) break;
        state.players[idx] = ps;
        ++idx;
    }

    // Zero-fill remaining slots
    for (; idx < MAX_PLAYERS; ++idx) {
        memset(&state.players[idx], 0, sizeof(PlayerState));
    }

    broadcastPacket(&state, sizeof(state), true);
}

// ---------------------------------------------------------------------------
// Accessor
// ---------------------------------------------------------------------------

Game& Server::getGame()
{
    return game;
}
