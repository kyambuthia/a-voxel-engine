#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Loads SPIR-V shader modules from compiled .spv files.
// -----------------------------------------------------------------------------
class VulkanShader {
public:
    // Load a .spv file and create a VkShaderModule
    static VkShaderModule load(VkDevice device, const std::string& filepath);
    static std::vector<char> readFile(const std::string& filepath);
};
