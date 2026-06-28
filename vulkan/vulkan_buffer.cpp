#include "vulkan_buffer.h"
#include <cassert>
#include <cstring>

VulkanBuffer::~VulkanBuffer() { cleanup(); }

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : m_context(other.m_context)
    , m_buffer(other.m_buffer)
    , m_memory(other.m_memory)
    , m_size(other.m_size)
    , m_mapped(other.m_mapped)
{
    other.m_context = nullptr;
    other.m_buffer  = VK_NULL_HANDLE;
    other.m_memory  = VK_NULL_HANDLE;
    other.m_size    = 0;
    other.m_mapped  = nullptr;
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_context = other.m_context;
        m_buffer  = other.m_buffer;
        m_memory  = other.m_memory;
        m_size    = other.m_size;
        m_mapped  = other.m_mapped;
        other.m_context = nullptr;
        other.m_buffer  = VK_NULL_HANDLE;
        other.m_memory  = VK_NULL_HANDLE;
        other.m_size    = 0;
        other.m_mapped  = nullptr;
    }
    return *this;
}

void VulkanBuffer::init(VulkanContext* ctx,
                         VkDeviceSize size,
                         VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties)
{
    m_context = ctx;
    m_size    = size;
    VkDevice device = ctx->device();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = ctx->findMemoryType(memReq.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &m_memory));

    vkBindBufferMemory(device, m_buffer, m_memory, 0);

    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        vkMapMemory(device, m_memory, 0, size, 0, &m_mapped);
    }
}

void VulkanBuffer::cleanup() {
    if (!m_context) return;
    VkDevice device = m_context->device();

    if (m_mapped) {
        vkUnmapMemory(device, m_memory);
        m_mapped = nullptr;
    }
    if (m_buffer) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_memory) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_context = nullptr;
    m_size    = 0;
}

void VulkanBuffer::upload(const void* data, VkDeviceSize size) {
    assert(m_mapped);
    memcpy(m_mapped, data, size);
}

void* VulkanBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
    VkDevice device = m_context->device();
    void* data;
    vkMapMemory(device, m_memory, offset, size, 0, &data);
    return data;
}

void VulkanBuffer::unmap() {
    VkDevice device = m_context->device();
    vkUnmapMemory(device, m_memory);
}

VulkanBuffer VulkanBuffer::createAndUpload(VulkanContext* ctx,
                                            const void* data,
                                            VkDeviceSize size,
                                            VkBufferUsageFlags usage)
{
    // Staging buffer
    VulkanBuffer staging;
    staging.init(ctx, size,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.upload(data, size);

    // Device-local buffer
    VulkanBuffer deviceBuf;
    deviceBuf.init(ctx, size,
                   usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Copy
    VkCommandBuffer cmd = ctx->beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, staging.buffer(), deviceBuf.buffer(), 1, &copyRegion);
    ctx->endSingleTimeCommands(cmd);

    staging.cleanup();
    return deviceBuf;
}
