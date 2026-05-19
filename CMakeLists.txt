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
    message(STATUS "raylib not found locally – fetching via FetchContent")
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan/raylib.git
        GIT_TAG        5.0
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

# ---- ENet ------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ENET QUIET libenet)
endif()

if(NOT ENET_FOUND)
    message(STATUS "ENet not found locally – fetching via FetchContent")
    FetchContent_Declare(
        enet
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG        v1.3.17
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
# Collect source files
# ---------------------------------------------------------------------------
set(KAIKAI_COMMON_SOURCES
    # Add shared sources here as the project grows
)

file(GLOB_RECURSE KAIKAI_SERVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
)

file(GLOB_RECURSE KAIKAI_CLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/client/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
)

# ---------------------------------------------------------------------------
# Server target
# ---------------------------------------------------------------------------
add_executable(kaikai_server ${KAIKAI_SERVER_SOURCES})
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
# Client target
# ---------------------------------------------------------------------------
add_executable(kaikai_client ${KAIKAI_CLIENT_SOURCES})
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
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
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
