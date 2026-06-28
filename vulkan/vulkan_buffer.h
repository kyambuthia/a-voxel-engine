#pragma once

#include "vulkan_context.h"

// -----------------------------------------------------------------------------
// RAII wrapper for VkBuffer / VkDeviceMemory.
// -----------------------------------------------------------------------------
class VulkanBuffer {
public:
    VulkanBuffer() = default;
    ~VulkanBuffer();

    // No copy
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // Move
    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    void init(VulkanContext* ctx,
              VkDeviceSize size,
              VkBufferUsageFlags usage,
              VkMemoryPropertyFlags properties);

    void cleanup();

    void upload(const void* data, VkDeviceSize size);
    void* map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void  unmap();

    VkBuffer       buffer()       const { return m_buffer; }
    VkDeviceMemory memory()       const { return m_memory; }
    VkDeviceSize   size()         const { return m_size; }
    bool           isMapped()     const { return m_mapped != nullptr; }

    // Create a staging buffer, upload data, and return a device-local buffer
    static VulkanBuffer createAndUpload(VulkanContext* ctx,
                                         const void* data,
                                         VkDeviceSize size,
                                         VkBufferUsageFlags usage);

private:
    VulkanContext*  m_context = nullptr;
    VkBuffer        m_buffer  = VK_NULL_HANDLE;
    VkDeviceMemory  m_memory  = VK_NULL_HANDLE;
    VkDeviceSize    m_size    = 0;
    void*           m_mapped  = nullptr;
};
