cmake_minimum_required(VERSION 3.16)
project(a-voxel-engine
    VERSION 0.1.0
    DESCRIPTION "Portable voxel engine (OpenGL 3.3 core / OpenGL ES 3.0)"
    LANGUAGES C CXX
)

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Dependencies ---
# SDL3 as a vendored submodule (windowing, input, GL context, Android shell).
add_subdirectory(dependencies/SDL)

# GLM as a vendored submodule (header-only math library).
add_subdirectory(dependencies/glm)

# Desktop: system OpenGL headers + driver. Android gets GLES from the NDK and
# SDL links it internally; all entry points are resolved at runtime via
# SDL_GL_GetProcAddress, so nothing beyond headers is strictly required.
if(NOT ANDROID)
    find_package(OpenGL REQUIRED)
endif()

# --- World core (coordinates, chunk storage, block lookup) ---
# A small static library so unit tests can link the world contract without
# SDL/GL. Pure C++17 standard library, no renderer types.
add_library(voxel_world STATIC
    world/world.cpp
    world/noise.cpp
)
target_include_directories(voxel_world PUBLIC ${CMAKE_SOURCE_DIR})
# Float-contract control: fuse-free math keeps noise results bit-identical
# across the desktop (GCC) and Android (Clang) toolchains, which the
# "same world seed -> same terrain hashes" acceptance gate depends on.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(voxel_world PRIVATE -ffp-contract=off)
endif()

# --- Sources ---
set(SOURCES
    src/main.cpp
    render/renderer.cpp
    render/opengl/gl_renderer.cpp
    render/opengl/gl_utils.cpp
)

set(HEADERS
    render/renderer.h
    render/opengl/gl_renderer.h
    render/opengl/gl_utils.h
    render/opengl/gl_shaders.h
)

# --- Target ---
# Android produces a shared library (.so) loaded by SDLActivity.java;
# all other platforms produce an executable.
if(ANDROID)
    add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
endif()

# --- Include directories ---
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

# --- Link libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    glm::glm
    voxel_world
)

if(NOT ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
endif()

# --- Compiler warnings (not NDK — it has its own flags) ---
if(NOT ANDROID AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(voxel_world PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Sanitizers (desktop debug builds only) ---
if(NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
    target_compile_options(voxel_world PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(voxel_world PRIVATE -fsanitize=address -fsanitize=undefined)
    message(STATUS "Sanitizers enabled: ASAN + UBSAN")
endif()

# --- Tests (desktop host only) ---
if(NOT ANDROID)
    enable_testing()
    add_executable(world_core_tests
        tests/test_main.cpp
        tests/world_core_test.cpp
        tests/noise_test.cpp
    )
    target_include_directories(world_core_tests PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(world_core_tests PRIVATE voxel_world)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(world_core_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(world_core_tests PRIVATE -ffp-contract=off)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(world_core_tests PRIVATE -fsanitize=address -fsanitize=undefined)
        target_link_options(world_core_tests PRIVATE -fsanitize=address -fsanitize=undefined)
    endif()
    add_test(NAME world_core COMMAND world_core_tests)
endif()
