#include "vulkan_mesh.h"
#include <array>

VulkanMesh::VulkanMesh(VulkanMesh&& other) noexcept
    : m_context(other.m_context)
    , m_vertexBuffer(std::move(other.m_vertexBuffer))
    , m_indexBuffer(std::move(other.m_indexBuffer))
    , m_indexCount(other.m_indexCount)
    , m_vertexCount(other.m_vertexCount)
{
    other.m_context    = nullptr;
    other.m_indexCount = 0;
    other.m_vertexCount = 0;
}

VulkanMesh& VulkanMesh::operator=(VulkanMesh&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_context      = other.m_context;
        m_vertexBuffer = std::move(other.m_vertexBuffer);
        m_indexBuffer  = std::move(other.m_indexBuffer);
        m_indexCount   = other.m_indexCount;
        m_vertexCount  = other.m_vertexCount;
        other.m_context    = nullptr;
        other.m_indexCount = 0;
        other.m_vertexCount = 0;
    }
    return *this;
}

void VulkanMesh::init(VulkanContext* ctx, const Mesh& mesh) {
    m_context    = ctx;
    m_indexCount  = static_cast<uint32_t>(mesh.indices.size());
    m_vertexCount = static_cast<uint32_t>(mesh.vertices.size());

    // Vertex buffer
    m_vertexBuffer = VulkanBuffer::createAndUpload(
        ctx, mesh.vertices.data(),
        mesh.vertices.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Index buffer
    m_indexBuffer = VulkanBuffer::createAndUpload(
        ctx, mesh.indices.data(),
        mesh.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void VulkanMesh::cleanup() {
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
    m_context    = nullptr;
    m_indexCount = 0;
    m_vertexCount = 0;
}

void VulkanMesh::bind(VkCommandBuffer cmd) const {
    VkBuffer vertexBuffers[] = { m_vertexBuffer.buffer() };
    VkDeviceSize offsets[]   = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer.buffer(), 0, VK_INDEX_TYPE_UINT32);
}

void VulkanMesh::draw(VkCommandBuffer cmd) const {
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}

// -----------------------------------------------------------------------------
// Cube mesh generation
// -----------------------------------------------------------------------------
Mesh VulkanMesh::makeCube(float s) {
    float h = s / 2.0f;

    // 24 unique vertices (4 per face, each with its own normal)
    // Color is not per-vertex; the fragment shader uses a push-constant color.
    std::vector<Vertex> verts = {
        // Front (+Z)
        {{-h, -h,  h}, { 0, 0, 1}},
        {{ h, -h,  h}, { 0, 0, 1}},
        {{ h,  h,  h}, { 0, 0, 1}},
        {{-h,  h,  h}, { 0, 0, 1}},
        // Back (-Z)
        {{ h, -h, -h}, { 0, 0,-1}},
        {{-h, -h, -h}, { 0, 0,-1}},
        {{-h,  h, -h}, { 0, 0,-1}},
        {{ h,  h, -h}, { 0, 0,-1}},
        // Top (+Y)
        {{-h,  h,  h}, { 0, 1, 0}},
        {{ h,  h,  h}, { 0, 1, 0}},
        {{ h,  h, -h}, { 0, 1, 0}},
        {{-h,  h, -h}, { 0, 1, 0}},
        // Bottom (-Y)
        {{-h, -h, -h}, { 0,-1, 0}},
        {{ h, -h, -h}, { 0,-1, 0}},
        {{ h, -h,  h}, { 0,-1, 0}},
        {{-h, -h,  h}, { 0,-1, 0}},
        // Right (+X)
        {{ h, -h,  h}, { 1, 0, 0}},
        {{ h, -h, -h}, { 1, 0, 0}},
        {{ h,  h, -h}, { 1, 0, 0}},
        {{ h,  h,  h}, { 1, 0, 0}},
        // Left (-X)
        {{-h, -h, -h}, {-1, 0, 0}},
        {{-h, -h,  h}, {-1, 0, 0}},
        {{-h,  h,  h}, {-1, 0, 0}},
        {{-h,  h, -h}, {-1, 0, 0}},
    };

    // 36 indices (6 faces × 2 tris × 3 indices)
    std::vector<uint32_t> idx = {
        0, 1, 2, 2, 3, 0,      // front
        4, 5, 6, 6, 7, 4,      // back
        8, 9, 10, 10, 11, 8,   // top
        12, 13, 14, 14, 15, 12, // bottom
        16, 17, 18, 18, 19, 16, // right
        20, 21, 22, 22, 23, 20  // left
    };

    return { verts, idx };
}
