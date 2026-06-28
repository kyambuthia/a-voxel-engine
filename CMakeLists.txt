cmake_minimum_required(VERSION 3.16)
project(a-voxel-engine
    VERSION 0.1.0
    DESCRIPTION "Custom voxel engine using Vulkan"
    LANGUAGES C CXX
)

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(Vulkan REQUIRED)

# SDL3 as a vendored submodule
add_subdirectory(dependencies/SDL)

# GLM as a vendored submodule (header-only math library)
add_subdirectory(dependencies/glm)

# --- Sources ---
set(VULKAN_SOURCES
    vulkan/vulkan_context.cpp
    vulkan/vulkan_pipeline.cpp
    vulkan/vulkan_shader.cpp
    vulkan/vulkan_buffer.cpp
    vulkan/vulkan_mesh.cpp
)

set(SOURCES
    src/main.cpp
    ${VULKAN_SOURCES}
)

set(VULKAN_HEADERS
    vulkan/vulkan_context.h
    vulkan/vulkan_pipeline.h
    vulkan/vulkan_shader.h
    vulkan/vulkan_buffer.h
    vulkan/vulkan_mesh.h
    vulkan/vulkan_types.h
)

# --- Target ---
# Android produces a shared library (.so) loaded by SDLActivity.java;
# all other platforms produce an executable.
if(ANDROID)
    add_library(${PROJECT_NAME} SHARED ${SOURCES} ${VULKAN_HEADERS})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${VULKAN_HEADERS})
endif()

# --- Include directories ---
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vulkan
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)

# --- Link libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${Vulkan_LIBRARIES}
    SDL3::SDL3
    glm::glm
)

# --- Compiler warnings (not NDK — it has its own flags) ---
if(NOT ANDROID AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Sanitizers (desktop debug builds only) ---
if(NOT ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
    message(STATUS "Sanitizers enabled: ASAN + UBSAN")
endif()

# --- Shaders: compile with glslc ---
file(GLOB SHADER_SOURCES shaders/*.vert shaders/*.frag)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPV_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND glslc -O -o ${SPV_OUTPUT} ${SHADER}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_NAME} -> ${SPV_OUTPUT}"
    )
    list(APPEND SHADER_SPVS ${SPV_OUTPUT})
endforeach()

# Make sure shaders are built before the target
if(SHADER_SPVS)
    add_custom_target(compile_shaders ALL DEPENDS ${SHADER_SPVS})
    add_dependencies(${PROJECT_NAME} compile_shaders)
endif()

# Copy compiled shaders to the output directory
# Desktop: next to the executable
# Android: into the Gradle assets dir for APK packaging
if(ANDROID)
    set(SHADER_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/android/app/src/main/assets/shaders")
else()
    set(SHADER_OUTPUT_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders")
endif()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/shaders"
        "${SHADER_OUTPUT_DIR}"
    COMMENT "Copying compiled shaders to ${SHADER_OUTPUT_DIR}"
)
