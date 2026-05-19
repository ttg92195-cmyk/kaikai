cmake_minimum_required(VERSION 3.14)
project(Kaikai VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

# ---- raylib ----------------------------------------------------------------
find_package(raylib QUIET 4.0)
if(NOT raylib_FOUND)
    message(STATUS "raylib not found locally - downloading release tarball")
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(
        raylib
        URL      https://github.com/raysan/raylib/releases/download/5.0/raylib-5.0.tar.gz
        URL_HASH MD5=c5c6e4c94a065fdee6b7c3c6c0c4b4c7
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES   OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

# ---- ENet ------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ENET QUIET libenet)
endif()

if(NOT ENET_FOUND)
    message(STATUS "ENet not found locally - downloading release tarball")
    FetchContent_Declare(
        enet
        URL      https://github.com/lsalzman/enet/archive/refs/tags/v1.3.17.tar.gz
        URL_HASH MD5=eed497f0d1bb8b85b6c3c3e42f9f7538
    )

    # ENet does not provide a CMakeLists.txt, so we build it ourselves
    FetchContent_GetProperties(enet)
    if(NOT enet_POPULATED)
        FetchContent_Populate(enet)
    endif()

    add_library(enet STATIC
        ${enet_SOURCE_DIR}/callbacks.c
        ${enet_SOURCE_DIR}/compress.c
        ${enet_SOURCE_DIR}/host.c
        ${enet_SOURCE_DIR}/list.c
        ${enet_SOURCE_DIR}/packet.c
        ${enet_SOURCE_DIR}/peer.c
        ${enet_SOURCE_DIR}/protocol.c
    )
    target_include_directories(enet PUBLIC ${enet_SOURCE_DIR}/include)
    target_compile_definitions(enet PUBLIC HAS_SOCKLEN_T=1)
    find_package(Threads REQUIRED)
    target_link_libraries(enet PUBLIC Threads::Threads)
    set(ENET_LIBRARIES enet)
    set(ENET_INCLUDE_DIRS ${enet_SOURCE_DIR}/include)
endif()

# ---------------------------------------------------------------------------
# Collect ALL source files
# ---------------------------------------------------------------------------
# Server needs: game, networking, ai, systems, audio, utils + main.cpp
file(GLOB_RECURSE KAIKAI_SERVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/networking/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ai/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/systems/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
)

# Client needs: game, networking, graphics, ai, systems, audio, utils + main.cpp
file(GLOB_RECURSE KAIKAI_CLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/networking/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ai/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/systems/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
)

# Both targets share main.cpp
set(KAIKAI_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# ---------------------------------------------------------------------------
# Server target (headless - no raylib needed for pure server)
# ---------------------------------------------------------------------------
add_executable(kaikai_server ${KAIKAI_SERVER_SOURCES} ${KAIKAI_MAIN})
target_include_directories(kaikai_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ENET_INCLUDE_DIRS}
)
target_link_libraries(kaikai_server PRIVATE
    ${ENET_LIBRARIES}
    Threads::Threads
    m
    dl
)
target_compile_definitions(kaikai_server PRIVATE KAIKAI_SERVER)

# ---------------------------------------------------------------------------
# Client target (needs raylib for graphics/audio)
# ---------------------------------------------------------------------------
add_executable(kaikai_client ${KAIKAI_CLIENT_SOURCES} ${KAIKAI_MAIN})
target_include_directories(kaikai_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ENET_INCLUDE_DIRS}
)
target_link_libraries(kaikai_client PRIVATE
    raylib
    ${ENET_LIBRARIES}
    Threads::Threads
    m
    dl
)
target_compile_definitions(kaikai_client PRIVATE KAIKAI_CLIENT)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable)
endif()

# ---------------------------------------------------------------------------
# Copy assets on post-build (client only)
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET kaikai_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:kaikai_client>/assets"
        COMMENT "Copying assets to output directory"
    )
endif()
