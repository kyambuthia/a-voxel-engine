#pragma once

#include "vulkan_types.h"
#include "vulkan_buffer.h"
#include "vulkan_context.h"

// -----------------------------------------------------------------------------
// GPU-side mesh: holds vertex + index buffers.
// -----------------------------------------------------------------------------
class VulkanMesh {
public:
    VulkanMesh() = default;
    ~VulkanMesh() { cleanup(); }

    // No copy
    VulkanMesh(const VulkanMesh&) = delete;
    VulkanMesh& operator=(const VulkanMesh&) = delete;

    // Move
    VulkanMesh(VulkanMesh&& other) noexcept;
    VulkanMesh& operator=(VulkanMesh&& other) noexcept;

    void init(VulkanContext* ctx, const Mesh& mesh);
    void cleanup();

    void bind(VkCommandBuffer cmd) const;
    void draw(VkCommandBuffer cmd) const;

    uint32_t indexCount() const { return m_indexCount; }
    uint32_t vertexCount() const { return m_vertexCount; }

    // Static helpers for common primitives
    static Mesh makeCube(float size);

private:
    VulkanContext* m_context = nullptr;
    VulkanBuffer   m_vertexBuffer;
    VulkanBuffer   m_indexBuffer;
    uint32_t       m_indexCount  = 0;
    uint32_t       m_vertexCount = 0;
};
