#include "vulkan_shader.h"
#include "vulkan_types.h"
#include <cassert>
#include <fstream>
#include <iostream>

std::vector<char> VulkanShader::readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FATAL] Failed to open shader file: " << filepath
                  << " at " << __FILE__ << ":" << __LINE__ << "\n";
        std::abort();
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule VulkanShader::load(VkDevice device, const std::string& filepath) {
    auto code = readFile(filepath);

    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &module));

    return module;
}
