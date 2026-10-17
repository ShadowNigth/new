cmake_minimum_required(VERSION 3.20)

project(ClassicFPS
    VERSION 1.0.0
    DESCRIPTION "ClassicFPS Engine - Native Linux FPS Engine inspired by GoldSrc / CS 1.6"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CLASSICFPS_BUILD_TESTS "Build ClassicFPS unit and integration tests" ON)
option(CLASSICFPS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Require Linux / POSIX target
if(NOT UNIX OR APPLE)
    message(WARNING "ClassicFPS Engine is designed primarily for native Linux targets.")
endif()

# Locate SDL2
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
endif()

# Optional link-time OpenGL (OpenGL 3.3 Core entry points are resolved dynamically via SDL_GL_GetProcAddress)
find_package(OpenGL QUIET)

set(CLASSICFPS_ENGINE_SOURCES
    engine/core/Version.cpp
    engine/core/Logger.cpp
    engine/core/Error.cpp
    engine/core/Clock.cpp
    engine/core/Config.cpp
    engine/core/ResourceManager.cpp
    engine/core/Engine.cpp
    engine/platform/Window.cpp
    engine/input/InputManager.cpp
    engine/physics/CollisionWorld.cpp
    engine/physics/FPSController.cpp
    engine/renderer/GL33Loader.cpp
    engine/renderer/Camera.cpp
    engine/renderer/Texture.cpp
    engine/renderer/Mesh.cpp
    engine/renderer/Shader.cpp
    engine/renderer/OpenGLGraphicsDevice.cpp
    engine/renderer/SoftwareGraphicsDevice.cpp
    engine/weapons/WeaponDef.cpp
    engine/weapons/WeaponSystem.cpp
    engine/weapons/ViewModelRenderer.cpp
    engine/animation/Skeleton.cpp
    engine/resources/ModelLoader.cpp
    engine/audio/AudioEngine.cpp
    engine/world/EntityManager.cpp
    engine/world/MapFormat.cpp
    engine/world/TestMap.cpp
    engine/ui/HudAndConsole.cpp
    engine/networking/NetSystem.cpp
    engine/debug/FrameStats.cpp
    engine/debug/Profiler.cpp
)

add_library(classicfps_engine STATIC ${CLASSICFPS_ENGINE_SOURCES})

target_include_directories(classicfps_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        /usr/local/include
)

if(TARGET SDL2::SDL2)
    target_link_libraries(classicfps_engine PUBLIC SDL2::SDL2)
elseif(TARGET PkgConfig::SDL2)
    target_link_libraries(classicfps_engine PUBLIC PkgConfig::SDL2)
else()
    target_include_directories(classicfps_engine PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(classicfps_engine PUBLIC ${SDL2_LIBRARIES})
endif()

if(OpenGL_FOUND AND TARGET OpenGL::GL)
    target_link_libraries(classicfps_engine PUBLIC OpenGL::GL)
endif()

target_link_libraries(classicfps_engine PUBLIC pthread dl)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(classicfps_engine PUBLIC
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
    if(CLASSICFPS_ENABLE_SANITIZERS)
        target_compile_options(classicfps_engine PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(classicfps_engine PUBLIC -fsanitize=address,undefined)
    endif()
endif()

# Main game executable
add_executable(classicfps_demo
    game/src/main.cpp
)
target_link_libraries(classicfps_demo PRIVATE classicfps_engine)

# Standalone Engine Tools (Phase 6)
add_executable(classicfps_asset_converter
    tools/asset_converter.cpp
)
target_link_libraries(classicfps_asset_converter PRIVATE classicfps_engine)

add_executable(classicfps_map_viewer
    tools/map_viewer.cpp
)
target_link_libraries(classicfps_map_viewer PRIVATE classicfps_engine)

# Unit & Integration Tests (CTest)
if(CLASSICFPS_BUILD_TESTS)
    enable_testing()

    add_executable(classicfps_tests
        tests/test_main.cpp
    )
    target_link_libraries(classicfps_tests PRIVATE classicfps_engine)

    add_test(NAME core.version                        COMMAND classicfps_tests version)
    add_test(NAME core.logger                         COMMAND classicfps_tests logger)
    add_test(NAME core.config                         COMMAND classicfps_tests config)
    add_test(NAME core.clock                          COMMAND classicfps_tests clock)
    add_test(NAME core.resource_manager               COMMAND classicfps_tests resource_manager)
    add_test(NAME debug.frame_stats                   COMMAND classicfps_tests frame_stats)
    add_test(NAME renderer.camera_and_frustum         COMMAND classicfps_tests camera_and_frustum)
    add_test(NAME renderer.textures_and_ppm           COMMAND classicfps_tests textures_and_ppm)
    add_test(NAME renderer.mesh_and_bounds            COMMAND classicfps_tests mesh_and_bounds)
    add_test(NAME renderer.shader_validation          COMMAND classicfps_tests shader_validation)
    add_test(NAME physics.collision_and_raycast       COMMAND classicfps_tests collision_and_raycast)
    add_test(NAME physics.fps_movement_and_jump       COMMAND classicfps_tests fps_movement_and_jump)
    add_test(NAME physics.crouch_and_slopes           COMMAND classicfps_tests crouch_and_slopes)
    add_test(NAME weapons.config_and_definitions      COMMAND classicfps_tests weapon_config)
    add_test(NAME weapons.state_machine_and_combat    COMMAND classicfps_tests weapon_state_machine)
    add_test(NAME weapons.viewmodel_and_impacts       COMMAND classicfps_tests weapon_viewmodel)
    add_test(NAME resources.gltf2_and_skeletal_anim   COMMAND classicfps_tests gltf_and_animation)
    add_test(NAME audio.wav_and_spatial_3d_mixer      COMMAND classicfps_tests audio_spatial)
    add_test(NAME world.entities_and_map_formats      COMMAND classicfps_tests entities_and_maps)
    add_test(NAME ui.hud_crosshair_and_console        COMMAND classicfps_tests hud_and_console)
    add_test(NAME networking.udp_prediction_reconcile COMMAND classicfps_tests networking_udp)
    add_test(NAME debug.profiler_and_lifecycle        COMMAND classicfps_tests engine_lifecycle)
endif()
