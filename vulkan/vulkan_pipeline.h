#pragma once

#include "vulkan_types.h"
#include "vulkan_context.h"
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Manages the graphics pipeline, descriptor set layout, and pipeline layout.
// -----------------------------------------------------------------------------
struct PipelineConfig {
    VkViewport viewport;
    VkRect2D   scissor;
    bool       enableDepthTest  = true;
    bool       enableDepthWrite = true;
    VkCompareOp depthCompareOp  = VK_COMPARE_OP_LESS;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygonMode   = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode    = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace        = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Shader paths (relative to executable)
    std::string vertShaderPath = "shaders/main.vert.spv";
    std::string fragShaderPath = "shaders/main.frag.spv";
};

class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void init(VulkanContext* ctx, const PipelineConfig& config);
    void cleanup();

    VkPipeline       pipeline()        const { return m_pipeline; }
    VkPipelineLayout pipelineLayout()  const { return m_pipelineLayout; }
    VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }
    const PipelineConfig& config() const { return m_config; }

    // Descriptor pool and sets
    void createDescriptorPool(uint32_t maxSets);
    void createDescriptorSets(uint32_t count);

    VkDescriptorPool  descriptorPool()  const { return m_descriptorPool; }
    VkDescriptorSet   descriptorSet(uint32_t i) const { return m_descriptorSets[i]; }
    std::vector<VkDescriptorSet>& descriptorSets() { return m_descriptorSets; }

private:
    VulkanContext*  m_context = nullptr;
    PipelineConfig  m_config;

    VkPipeline       m_pipeline        = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout  = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool      = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
};
