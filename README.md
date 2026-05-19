# Kaikai - 3D Multiplayer Horror Game

A co-op horror game where **4 survivors** attempt to escape a haunted location while **1 player** stalks them as a ghost. Built with C++17, raylib, and ENet.

---

## Features

### Core Gameplay
- **Asymmetric Multiplayer** – 4 survivors vs. 1 ghost in each match
- **Escape Objective** – Survivors must find keys, solve puzzles, and reach the exit before the ghost catches them
- **Ghost Mechanics** – The ghost can phase through walls, go invisible, and trigger jumpscares

### Survivor Systems
- **Flashlight & Battery** – Toggle your flashlight to see in the dark; battery drains while active and flickers when low
- **Stamina System** – Sprint to outrun the ghost, but manage your stamina carefully—once depleted you slow to a walk
- **Sanity System** – Stay in the dark too long and your sanity drops, causing visual distortions and audio hallucinations
- **Proximity Voice Chat** – Communicate with nearby survivors; the ghost can hear you too

### Ghost Systems
- **A* Pathfinding** – The ghost navigates the map using A* on a 2D grid overlay
- **Invisibility** – After a short cooldown the ghost can turn invisible and stalk survivors
- **Jumpscare System** – Trigger terrifying jumpscares when catching a survivor
- **Sound Detection** – Running, talking, and interacting with objects makes noise the ghost can detect

### World Systems
- **Fog of War** – Dense fog limits visibility and adds atmosphere
- **Random Item Spawning** – Batteries, keys, and tools spawn in randomized locations each match
- **Doors & Switches** – Interact with the environment to open doors, flip switches, and unlock paths
- **Spectator Mode** – Dead survivors can float around and watch the remaining players

### Technical
- **Client-Server Architecture** – Authoritative server with ENet-based networking
- **60 Hz Tick Rate** – Smooth server simulation with 20 Hz network update rate
- **Cross-Platform** – Builds on Linux, macOS, and Windows

---

## Project Structure

```
kaikai/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── .gitignore
├── .github/
│   └── workflows/
│       └── build.yml           # CI pipeline
├── assets/                     # Textures, models, sounds, fonts
│   ├── textures/
│   ├── models/
│   ├── sounds/
│   └── fonts/
└── src/
    ├── client/                 # Client-side code
    │   ├── ClientMain.cpp
    │   ├── Renderer.cpp
    │   └── InputHandler.cpp
    ├── server/                 # Server-side code
    │   ├── ServerMain.cpp
    │   ├── GameServer.cpp
    │   └── Pathfinding.cpp
    ├── shared/                 # Shared game logic
    │   ├── PlayerState.h
    │   ├── NetworkProtocol.h
    │   └── GameState.h
    └── utils/                  # Utility headers
        ├── Constants.h
        ├── Math.h
        └── Logger.h
```

---

## Building

### Prerequisites

| Dependency   | Version | Notes                                     |
|-------------|---------|-------------------------------------------|
| CMake       | ≥ 3.14  | Build system                              |
| C++ Compiler| C++17   | GCC ≥ 9, Clang ≥ 10, MSVC ≥ 2019        |
| raylib      | ≥ 5.0   | Fetched automatically if not installed    |
| ENet        | ≥ 1.3   | Fetched automatically if not installed    |

### Linux

```bash
# Install system dependencies (Debian/Ubuntu)
sudo apt-get install build-essential cmake pkg-config \
    libglfw3-dev libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libxext-dev

# Optional: install raylib and ENet from packages
sudo apt-get install libraylib-dev libenet-dev

# Build
git clone https://github.com/your-org/kaikai.git
cd kaikai
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run
./kaikai_server          # Start the server
./kaikai_client          # Start the client
```

### macOS

```bash
brew install cmake raylib enet
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Windows

```powershell
# With vcpkg
vcpkg install raylib enet
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

---

## How to Play

1. **Start the server** – One player runs `kaikai_server` on the host machine (or a dedicated server).
2. **Connect clients** – Up to 5 players run `kaikai_client` and connect to the server IP.
3. **Role assignment** – One player is randomly assigned the ghost role; the rest are survivors.
4. **Survivors** – Explore the map, collect items, find keys, and escape through the exit door.
5. **Ghost** – Hunt the survivors. Use invisibility, pathfinding, and jumpscares to catch them.
6. **Win condition** – Survivors win if at least one reaches the exit. The ghost wins if all survivors are caught.

---

## Controls

| Key          | Action                          |
|-------------|----------------------------------|
| **W/A/S/D** | Move forward / left / back / right |
| **Shift**   | Sprint (drains stamina)          |
| **F**       | Toggle flashlight                |
| **E**       | Interact (doors, items, switches)|
| **Tab**     | Hold to activate voice chat      |
| **Space**   | Jump (survivor) / Ascend (spectator) |
| **C**       | Descend (spectator mode only)    |
| **Esc**     | Pause / Open menu                |

---

## Configuration

Key gameplay parameters can be tuned in `src/utils/Constants.h`:

| Constant                      | Default | Description                           |
|------------------------------|---------|---------------------------------------|
| `MAX_PLAYERS`                | 5       | Maximum players per match             |
| `TICK_RATE`                  | 60      | Server simulation ticks per second    |
| `NETWORK_UPDATE_RATE`        | 20      | Network packets sent per second       |
| `SERVER_PORT`                | 7777    | Default server port                   |
| `STAMINA_DRAIN_RATE`         | 10.0    | Stamina lost per second while running |
| `BATTERY_DRAIN_RATE`         | 3.0     | Battery lost per second with light on |
| `SANITY_DRAIN_RATE`          | 2.0     | Sanity lost per second in the dark    |
| `GHOST_SPEED`                | 4.5     | Ghost movement speed                  |
| `PLAYER_WALK_SPEED`          | 3.0     | Survivor walking speed                |
| `PLAYER_RUN_SPEED`           | 5.5     | Survivor sprinting speed              |
| `VOICE_CHAT_MAX_DISTANCE`    | 30.0    | Voice chat audible range (world units)|

---

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.
