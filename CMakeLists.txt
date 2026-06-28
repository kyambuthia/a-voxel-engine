cmake_minimum_required(VERSION 3.16)
project(a-raylib-demo
    VERSION 0.1.0
    DESCRIPTION "Voxel demo with raylib"
    LANGUAGES C CXX
)

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- raylib configuration (set before add_subdirectory) ---
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WITH_PIC ON CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_TYPE "STATIC" CACHE STRING "" FORCE)

# --- raylib (subdirectory build) ---
add_subdirectory(${CMAKE_SOURCE_DIR}/dependencies/raylib)

# --- Sources ---
set(SOURCES
    src/main.cpp
)

# --- Executable ---
add_executable(${PROJECT_NAME} ${SOURCES})

# --- Include directories ---
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dependencies/raylib/src
)

# --- Link libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# --- Platform-specific ---
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32)
endif()

# --- Compiler warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
