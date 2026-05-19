cmake_minimum_required(VERSION 3.14)
project(Kaikai VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# ---- Threads (needed by all targets and ENet) -----------------------------
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Collect ALL source files (shared between desktop and Android)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE KAIKAI_SERVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/networking/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ai/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/systems/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/*.cpp"
)

file(GLOB_RECURSE KAIKAI_CLIENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/networking/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ai/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/systems/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
)

set(KAIKAI_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable)
endif()

# ===========================================================================
# DESKTOP BUILD (Linux)
# ===========================================================================
if(NOT ANDROID)

    # ---- raylib (desktop) ---------------------------------------------------
    # First: try CMake find_package (works if raylib was installed via cmake install)
    find_package(raylib QUIET)

    if(NOT raylib_FOUND)
        # Second: try pkg-config (some installs only register via pkg-config)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(RAYLIB QUIET raylib)
        endif()

        if(RAYLIB_FOUND)
            message(STATUS "raylib found via pkg-config: ${RAYLIB_VERSION}")
            # Create an imported target from pkg-config results
            add_library(raylib::raylib UNKNOWN IMPORTED)
            find_library(RAYLIB_LIB_PATH NAMES raylib PATHS ${RAYLIB_LIBRARY_DIRS} /usr/local/lib)
            find_path(RAYLIB_INC_PATH NAMES raylib.h PATHS ${RAYLIB_INCLUDE_DIRS} /usr/local/include)
            set_target_properties(raylib::raylib PROPERTIES
                IMPORTED_LOCATION "${RAYLIB_LIB_PATH}"
                INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INC_PATH}"
            )
            set(raylib_FOUND TRUE)
        else()
            message(FATAL_ERROR
                "raylib NOT found!\n"
                "Install with: sudo apt-get install libraylib-dev\n"
                "Or build from source: https://github.com/raysan5/raylib\n"
                "CMAKE_PREFIX_PATH was: ${CMAKE_PREFIX_PATH}"
            )
        endif()
    else()
        message(STATUS "raylib found via CMake find_package: ${raylib_VERSION}")
        # find_package(raylib) creates target 'raylib', not 'raylib::raylib'
        # Create an alias so we can consistently use raylib::raylib everywhere
        if(NOT TARGET raylib::raylib)
            add_library(raylib::raylib ALIAS raylib)
        endif()
    endif()

    # ---- ENet (desktop) -----------------------------------------------------
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ENET QUIET libenet)
    endif()

    if(NOT ENET_FOUND)
        message(STATUS "ENet not found locally - fetching via FetchContent")
        include(FetchContent)
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
        target_link_libraries(enet PUBLIC Threads::Threads)
        set(ENET_LIBRARIES enet)
        set(ENET_INCLUDE_DIRS ${enet_SOURCE_DIR}/include)
    endif()

    # ---- Server target (headless) -------------------------------------------
    add_executable(kaikai_server ${KAIKAI_SERVER_SOURCES} ${KAIKAI_MAIN})
    target_include_directories(kaikai_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ENET_INCLUDE_DIRS}
    )
    target_link_libraries(kaikai_server PRIVATE
        raylib::raylib
        ${ENET_LIBRARIES}
        Threads::Threads
        m
        dl
    )
    target_compile_definitions(kaikai_server PRIVATE KAIKAI_SERVER)

    # ---- Client target (needs raylib for graphics/audio) --------------------
    add_executable(kaikai_client ${KAIKAI_CLIENT_SOURCES} ${KAIKAI_MAIN})
    target_include_directories(kaikai_client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ENET_INCLUDE_DIRS}
    )
    target_link_libraries(kaikai_client PRIVATE
        raylib::raylib
        ${ENET_LIBRARIES}
        Threads::Threads
        m
        dl
    )
    target_compile_definitions(kaikai_client PRIVATE KAIKAI_CLIENT)

    # ---- Copy assets on post-build (client only) ----------------------------
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        add_custom_command(TARGET kaikai_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:kaikai_client>/assets"
            COMMENT "Copying assets to output directory"
        )
    endif()

endif() # NOT ANDROID (desktop build)

# ===========================================================================
# ANDROID BUILD TARGET
# ===========================================================================
# When building with the Android NDK toolchain (ANDROID=TRUE), we build a
# shared library (libkaikai_android.so) instead of desktop executables.
# This .so is loaded by android.app.NativeActivity at runtime.
# ===========================================================================
if(ANDROID)
    message(STATUS "=== Configuring Android build ===")

    # ---- raylib (Android) ---------------------------------------------------
    # raylib should have been built & installed for Android already.
    # The install prefix is passed via CMAKE_PREFIX_PATH.
    find_package(raylib REQUIRED)
    if(NOT TARGET raylib::raylib)
        add_library(raylib::raylib ALIAS raylib)
    endif()
    message(STATUS "raylib for Android found: ${raylib_VERSION}")

    # ---- native_app_glue (required by android.app.NativeActivity) -----------
    # native_app_glue provides ANativeActivity_onCreate and the app thread
    # that calls android_main (provided by raylib's platform_android code).
    # We link it separately since raylib's static lib doesn't include it.
    if(NOT ANDROID_NDK)
        if(DEFINED ENV{ANDROID_NDK_HOME})
            set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
        elseif(DEFINED ENV{ANDROID_NDK})
            set(ANDROID_NDK $ENV{ANDROID_NDK})
        else()
            message(FATAL_ERROR "ANDROID_NDK not set - cannot find native_app_glue")
        endif()
    endif()

    # NDK r23+ moved native_app_glue to a CMake package; older NDKs use source files
    find_package(native_app_glue QUIET CONFIG)
    if(NOT native_app_glue_FOUND)
        message(STATUS "Building native_app_glue from NDK source: ${ANDROID_NDK}")
        if(EXISTS "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
            add_library(native_app_glue STATIC
                ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
            )
            target_include_directories(native_app_glue PUBLIC
                ${ANDROID_NDK}/sources/android/native_app_glue
            )
        else()
            # NDK r23+: use the CMake-installed version
            message(FATAL_ERROR
                "native_app_glue not found! Check NDK version and path: ${ANDROID_NDK}")
        endif()
        add_library(native_app_glue::native_app_glue ALIAS native_app_glue)
    endif()

    # ---- ENet for Android (include sources directly in shared library) ------
    include(FetchContent)
    FetchContent_Declare(
        enet_android
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG        v1.3.17
    )
    FetchContent_GetProperties(enet_android)
    if(NOT enet_android_POPULATED)
        FetchContent_Populate(enet_android)
    endif()
    # Build ENet sources directly into the shared library (avoids static lib linking issues)
    set(ENET_ANDROID_SOURCES
        ${enet_android_SOURCE_DIR}/callbacks.c
        ${enet_android_SOURCE_DIR}/compress.c
        ${enet_android_SOURCE_DIR}/host.c
        ${enet_android_SOURCE_DIR}/list.c
        ${enet_android_SOURCE_DIR}/packet.c
        ${enet_android_SOURCE_DIR}/peer.c
        ${enet_android_SOURCE_DIR}/protocol.c
        ${enet_android_SOURCE_DIR}/unix.c
    )

    # ---- Android shared library target --------------------------------------
    add_library(kaikai_android SHARED
        ${KAIKAI_CLIENT_SOURCES}
        ${KAIKAI_MAIN}
        ${ENET_ANDROID_SOURCES}
    )
    target_include_directories(kaikai_android PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${enet_android_SOURCE_DIR}/include
    )
    target_compile_definitions(kaikai_android PRIVATE
        KAIKAI_CLIENT
        HAS_SOCKLEN_T=1
    )
    target_link_libraries(kaikai_android PRIVATE
        raylib::raylib
        native_app_glue::native_app_glue
        Threads::Threads
        android
        log
        EGL
        GLESv3
        OpenSLES
        m
    )

    # Don't strip all symbols - ANativeActivity_onCreate must remain visible!
    # Only strip debug info for Release builds.
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(kaikai_android PROPERTIES
            LINK_FLAGS "-Wl,--strip-debug"
        )
    endif()

    # Ensure the shared library exports all needed symbols
    set_target_properties(kaikai_android PROPERTIES
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )

    message(STATUS "=== Android build configured ===")
endif()
