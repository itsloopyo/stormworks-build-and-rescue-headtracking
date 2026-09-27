cmake_minimum_required(VERSION 3.21)

# Static CRT so the shipped opengl32.dll has no VC++ redistributable dependency.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(StormworksHeadTracking VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "StormworksHeadTracking is Windows-only (opengl32 proxy).")
endif()

# Shared CameraUnlock C++ core (UDP receiver, processing pipeline, hotkeys, INI).
set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/StormworksHeadTracking)

add_library(opengl32 SHARED
    ${SRC_DIR}/dllmain.cpp
    ${SRC_DIR}/gl_proxy.cpp
    ${SRC_DIR}/mod.cpp
    ${SRC_DIR}/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_config/legacy_config.cpp
    ${SRC_DIR}/view_rewrite.cpp
    ${SRC_DIR}/camera_uniforms.cpp
    ${SRC_DIR}/hud_reticle.cpp
    ${SRC_DIR}/frustum_hook.cpp
    ${SRC_DIR}/builds/build_registry.cpp
    ${SRC_DIR}/builds/steam_offsets.cpp
)

# Ship as opengl32.dll with no lib prefix.
set_target_properties(opengl32 PROPERTIES PREFIX "" OUTPUT_NAME "opengl32")
target_include_directories(opengl32 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(opengl32 PRIVATE cameraunlock cameraunlock_hooks ws2_32)

target_compile_definitions(opengl32 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

if(MSVC)
    target_compile_options(opengl32 PRIVATE /W4 $<$<CONFIG:Release>:/O2>)
endif()

# Characterization tests for the matrix maths and the config file. A normal
# target, so `pixi run build` compiles it and `pixi run test` runs it. It links
# the core library but no GL, no game and no hooks.
add_executable(StormworksHeadTrackingTests
    tests/test_main.cpp
    ${SRC_DIR}/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_config/legacy_config.cpp
    ${SRC_DIR}/view_rewrite.cpp
    ${SRC_DIR}/camera_uniforms.cpp
    ${SRC_DIR}/hud_reticle.cpp
)
target_include_directories(StormworksHeadTrackingTests PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(StormworksHeadTrackingTests PRIVATE cameraunlock ws2_32)
target_compile_definitions(StormworksHeadTrackingTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
set_target_properties(StormworksHeadTrackingTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
if(MSVC)
    target_compile_options(StormworksHeadTrackingTests PRIVATE /W4)
endif()

# The config differential test. The oracle is the dev pre-release's reader and hotkey registration
# with the core sources they compiled, copied verbatim into tests/config_differential/oracle. It is
# two libraries because RegisterHotkeys has to run on oracle_fake's recording poller; each renames
# the two namespaces its own way, so neither collides with the other or with the current core.
set(SW_ORACLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential/oracle)
add_library(sw_config_oracle STATIC
    ${SW_ORACLE_DIR}/src/config.cpp
    ${SW_ORACLE_DIR}/core/src/config/ini_reader.cpp
    ${SW_ORACLE_DIR}/core/src/config/value_guards.cpp
    ${SW_ORACLE_DIR}/core/src/logging/file_log.cpp
    tests/config_differential/oracle_config_adapter.cpp
)
target_include_directories(sw_config_oracle PRIVATE
    ${SW_ORACLE_DIR}/src
    ${SW_ORACLE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential)
target_compile_definitions(sw_config_oracle PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    cameraunlock=sw_oracle_core
    stormworks_ht=sw_oracle)

add_library(sw_hotkeys_oracle STATIC
    tests/config_differential/oracle_hotkeys_adapter.cpp
)
target_include_directories(sw_hotkeys_oracle PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential/oracle_fake
    ${SW_ORACLE_DIR}/src
    ${SW_ORACLE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential)
target_compile_definitions(sw_hotkeys_oracle PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    cameraunlock=sw_oracle_hk_core
    stormworks_ht=sw_oracle_hk)

add_executable(StormworksConfigDifferentialTests
    tests/config_differential/differential_tests.cpp
    ${SRC_DIR}/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_config/legacy_config.cpp
)
target_include_directories(StormworksConfigDifferentialTests PRIVATE
    ${SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential)
target_compile_definitions(StormworksConfigDifferentialTests PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    SW_DIFFERENTIAL_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential/data")
target_link_libraries(StormworksConfigDifferentialTests PRIVATE cameraunlock sw_config_oracle sw_hotkeys_oracle ws2_32)
set_target_properties(StormworksConfigDifferentialTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
if(MSVC)
    target_compile_options(StormworksConfigDifferentialTests PRIVATE /W4)
endif()
