#include "vulkan_context.h"
#include "vulkan_pipeline.h"
#include "vulkan_mesh.h"
#include "vulkan_buffer.h"
#include "vulkan_types.h"
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <string>
#include <libgen.h>
#include <filesystem>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// -----------------------------------------------------------------------------
// Window callbacks
// -----------------------------------------------------------------------------
static void onResize(GLFWwindow* window, int /*w*/, int /*h*/) {
    auto ctx = static_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
    if (ctx) ctx->recreateSwapchain();
}

// -----------------------------------------------------------------------------
// Returns the directory containing the executable
// -----------------------------------------------------------------------------
static std::string getExeDir() {
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return std::filesystem::absolute(p).parent_path().string();
    }
    return ".";
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    std::string exeDir = getExeDir();
    // --- GLFW init ---
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const int WIDTH = 1280, HEIGHT = 720;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "a-voxel-engine", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // --- Vulkan context ---
    VulkanContext ctx;
    ctx.init(window);
    glfwSetWindowUserPointer(window, &ctx);
    glfwSetFramebufferSizeCallback(window, onResize);

    // --- Pipeline ---
    PipelineConfig pipeConfig{};
    pipeConfig.viewport = { 0.0f, 0.0f,
                            static_cast<float>(ctx.swapchainExtent().width),
                            static_cast<float>(ctx.swapchainExtent().height),
                            0.0f, 1.0f };
    pipeConfig.scissor  = { {0, 0}, ctx.swapchainExtent() };
    pipeConfig.vertShaderPath = exeDir + "/shaders/main.vert.spv";
    pipeConfig.fragShaderPath = exeDir + "/shaders/main.frag.spv";

    VulkanPipeline pipeline;
    pipeline.init(&ctx, pipeConfig);
    pipeline.createDescriptorPool(ctx.imageCount());
    pipeline.createDescriptorSets(ctx.imageCount());

    // --- Mesh: a single voxel cube ---
    Mesh cubeData = VulkanMesh::makeCube(1.0f);
    VulkanMesh cube;
    cube.init(&ctx, cubeData);

    // --- Uniform buffers (one per frame) ---
    std::vector<VulkanBuffer> uniformBuffers(ctx.imageCount());
    for (uint32_t i = 0; i < ctx.imageCount(); ++i) {
        uniformBuffers[i].init(&ctx, sizeof(UniformBufferObject),
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    // --- Update descriptor sets ---
    for (uint32_t i = 0; i < ctx.imageCount(); ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i].buffer();
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = pipeline.descriptorSet(i);
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    }

    // --- Command buffers ---
    std::vector<VkCommandBuffer> commandBuffers(ctx.imageCount());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = ctx.commandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = ctx.imageCount();

    VkResult result = vkAllocateCommandBuffers(ctx.device(), &allocInfo, commandBuffers.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers.\n";
        return EXIT_FAILURE;
    }

    // --- Camera state ---
    glm::vec3 camPos    = glm::vec3(2.5f, 2.0f, 2.5f);
    glm::vec3 camTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp     = glm::vec3(0.0f, 1.0f, 0.0f);

    float rotationAngle = 0.0f;
    auto startTime = std::chrono::high_resolution_clock::now();

    // --- Voxel push color ---
    glm::vec3 voxelColor(0.8f, 0.3f, 0.1f); // warm orange-brown

    // --- Main loop ---
    std::cout << "Entering main loop.\n";
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // --- Update UBO ---
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(
            currentTime - startTime).count();

        // Slow rotation
        rotationAngle = time * 0.3f;

        // Camera orbit
        float radius = 3.5f;
        camPos.x = radius * cos(rotationAngle);
        camPos.z = radius * sin(rotationAngle);

        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.view  = glm::lookAt(camPos, camTarget, camUp);
        ubo.proj  = glm::perspective(glm::radians(60.0f),
                                     static_cast<float>(ctx.swapchainExtent().width) /
                                     static_cast<float>(ctx.swapchainExtent().height),
                                     0.1f, 100.0f);
        ubo.proj[1][1] *= -1.0f; // Vulkan Y-flip

        // --- Begin frame ---
        if (!ctx.beginFrame()) continue;

        // Upload UBO for the swapchain image we just acquired
        uniformBuffers[ctx.currentSwapchainImage()].upload(&ubo, sizeof(ubo));

        // --- Record command buffer ---
        VkCommandBuffer cmd = commandBuffers[ctx.currentSwapchainImage()];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkResetCommandBuffer(cmd, 0);
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Render pass
        VkClearValue clearValues[2];
        clearValues[0].color        = { { 0.1f, 0.1f, 0.15f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = ctx.renderPass();
        rpInfo.framebuffer       = ctx.framebuffer(ctx.currentSwapchainImage());
        rpInfo.renderArea        = { {0, 0}, ctx.swapchainExtent() };
        rpInfo.clearValueCount   = 2;
        rpInfo.pClearValues      = clearValues;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline());

        // Update viewport/scissor in case of resize
        VkViewport viewport = { 0.0f, 0.0f,
                                static_cast<float>(ctx.swapchainExtent().width),
                                static_cast<float>(ctx.swapchainExtent().height),
                                0.0f, 1.0f };
        VkRect2D scissor = { {0, 0}, ctx.swapchainExtent() };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Bind descriptors
        VkDescriptorSet currentDescSet = pipeline.descriptorSet(ctx.currentSwapchainImage());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipeline.pipelineLayout(),
                                 0, 1, &currentDescSet,
                                 0, nullptr);

        // Push voxel color
        vkCmdPushConstants(cmd, pipeline.pipelineLayout(),
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(glm::vec3), &voxelColor);

        // Draw the cube
        cube.bind(cmd);
        cube.draw(cmd);

        vkCmdEndRenderPass(cmd);

        VkResult endResult = vkEndCommandBuffer(cmd);
        if (endResult != VK_SUCCESS) {
            std::cerr << "Failed to end command buffer.\n";
            break;
        }

        // --- Submit ---
        ctx.submitFrame(cmd);
    }

    ctx.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Shutdown complete.\n";
    return EXIT_SUCCESS;
}
