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
    ${SRC_DIR}/view_rewrite.cpp
    ${SRC_DIR}/camera_uniforms.cpp
    ${SRC_DIR}/hud_reticle.cpp
    ${SRC_DIR}/frustum_hook.cpp
    ${SRC_DIR}/builds/build_registry.cpp
    ${SRC_DIR}/builds/steam_offsets.cpp
)

# Ship as opengl32.dll with no lib prefix.
set_target_properties(opengl32 PROPERTIES PREFIX "" OUTPUT_NAME "opengl32")

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
    ${SRC_DIR}/view_rewrite.cpp
    ${SRC_DIR}/camera_uniforms.cpp
    ${SRC_DIR}/hud_reticle.cpp
)
target_include_directories(StormworksHeadTrackingTests PRIVATE ${SRC_DIR})
target_link_libraries(StormworksHeadTrackingTests PRIVATE cameraunlock ws2_32)
target_compile_definitions(StormworksHeadTrackingTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
set_target_properties(StormworksHeadTrackingTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
if(MSVC)
    target_compile_options(StormworksHeadTrackingTests PRIVATE /W4)
endif()
