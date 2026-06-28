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
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)

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

# --- Executable ---
add_executable(${PROJECT_NAME} ${SOURCES} ${VULKAN_HEADERS})

# --- Include directories ---
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vulkan
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW3_INCLUDE_DIRS}
    /usr/include
)

# --- Link libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${Vulkan_LIBRARIES}
    ${GLFW3_LIBRARIES}
)

# --- Compiler warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Custom command: compile shaders with glslc ---
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

# Make sure shaders are built before the executable
if(SHADER_SPVS)
    add_custom_target(compile_shaders ALL DEPENDS ${SHADER_SPVS})
    add_dependencies(${PROJECT_NAME} compile_shaders)
endif()

# Copy compiled shaders to the build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMENT "Copying compiled shaders to target directory"
)
