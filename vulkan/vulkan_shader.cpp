#include "vulkan_shader.h"
#include "vulkan_types.h"
#include <SDL3/SDL_iostream.h>
#include <iostream>

std::vector<char> VulkanShader::readFile(const std::string& filepath) {
    size_t fileSize = 0;
    void* data = SDL_LoadFile(filepath.c_str(), &fileSize);
    if (!data) {
        std::cerr << "[FATAL] Failed to open shader file: " << filepath
                  << " — " << SDL_GetError()
                  << " at " << __FILE__ << ":" << __LINE__ << "\n";
        std::abort();
    }

    std::vector<char> buffer(static_cast<const char*>(data),
                             static_cast<const char*>(data) + fileSize);
    SDL_free(data);
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
