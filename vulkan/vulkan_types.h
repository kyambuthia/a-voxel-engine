#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <cstdint>
#include <cassert>
#include <iostream>

// -----------------------------------------------------------------------------
// VK_CHECK — calls a Vulkan function, logs + asserts on failure.
// Use as:  VK_CHECK(vkCreateInstance(...));
// -----------------------------------------------------------------------------
#define VK_CHECK(f)                                                       \
    do {                                                                  \
        VkResult _vr = (f);                                               \
        if (_vr != VK_SUCCESS) {                                          \
            std::cerr << "[VK_ERROR] " << #f << " returned " << _vr       \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n";   \
            std::abort();                                                 \
        }                                                                 \
    } while (0)

// -----------------------------------------------------------------------------
// Vertex layout
// -----------------------------------------------------------------------------
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;

    static VkVertexInputBindingDescription bindingDescription() {
        VkVertexInputBindingDescription d{};
        d.binding   = 0;
        d.stride    = sizeof(Vertex);
        d.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return d;
    }

    static std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};
        // position
        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(Vertex, pos);
        // normal
        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset   = offsetof(Vertex, normal);
        return attrs;
    }
};

// -----------------------------------------------------------------------------
// Uniform buffer object (matches layout in shader)
// -----------------------------------------------------------------------------
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// -----------------------------------------------------------------------------
// Mesh data (CPU side)
// -----------------------------------------------------------------------------
struct Mesh {
    std::vector<Vertex>     vertices;
    std::vector<uint32_t>   indices;
};
