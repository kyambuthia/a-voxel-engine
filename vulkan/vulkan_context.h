#pragma once

#include "vulkan_types.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <memory>

// -----------------------------------------------------------------------------
// VulkanContext owns the instance, physical/logical device, swapchain,
// render passes, framebuffers, command pool, and sync primitives.
// -----------------------------------------------------------------------------
class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    // No copy / move
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Initialization
    void init(GLFWwindow* window);
    void cleanup();

    // For framebuffer size queries
    GLFWwindow* window() const { return m_window; }

    // Frame acquire / present
    bool beginFrame();
    void submitFrame(VkCommandBuffer cmd);

    // Helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);

    // Accessors
    VkDevice             device()       const { return m_device; }
    VkPhysicalDevice     physicalDevice() const { return m_physicalDevice; }
    VkSwapchainKHR       swapchain()    const { return m_swapchain; }
    VkExtent2D           swapchainExtent() const { return m_swapchainExtent; }
    VkFormat             swapchainFormat() const { return m_swapchainFormat; }
    VkRenderPass         renderPass()   const { return m_renderPass; }
    VkCommandPool        commandPool()  const { return m_commandPool; }
    VkQueue              graphicsQueue() const { return m_graphicsQueue; }
    VkQueue              presentQueue() const { return m_presentQueue; }
    uint32_t             graphicsFamily() const { return m_graphicsFamily; }
    VkSampleCountFlagBits msaaSamples()  const { return m_msaaSamples; }
    VkImageView          depthImageView() const { return m_depthImageView; }

    // Number of swapchain images
    uint32_t imageCount() const { return static_cast<uint32_t>(m_swapchainImageViews.size()); }

    // Current swapchain image index (valid after beginFrame)
    uint32_t currentSwapchainImage() const { return m_currentSwapchainImage; }

    // Framebuffer for a given swapchain image index
    VkFramebuffer framebuffer(uint32_t index) const { return m_swapchainFramebuffers[index]; }

    // Recreate swapchain (e.g. on window resize).
    // Destroys and recreates framebuffers, depth resources, image views,
    // swapchain, AND sync objects.  Caller must reallocate any per-image
    // resources (uniform buffers, command buffers, descriptor sets).
    void recreateSwapchain();

private:
    // --- Window ---
    GLFWwindow*      m_window            = nullptr;

    // --- Instance & Debug ---
    VkInstance       m_instance          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    bool             m_enableValidation  = true;

    // --- Device ---
    VkPhysicalDevice m_physicalDevice    = VK_NULL_HANDLE;
    VkDevice         m_device            = VK_NULL_HANDLE;
    VkQueue          m_graphicsQueue     = VK_NULL_HANDLE;
    VkQueue          m_presentQueue      = VK_NULL_HANDLE;
    uint32_t         m_graphicsFamily    = UINT32_MAX;
    uint32_t         m_presentFamily     = UINT32_MAX;

    // --- Surface ---
    VkSurfaceKHR     m_surface           = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_surfaceFormat   = {};

    // --- Swapchain ---
    VkSwapchainKHR   m_swapchain         = VK_NULL_HANDLE;
    VkExtent2D       m_swapchainExtent   = {};
    VkFormat         m_swapchainFormat   = VK_FORMAT_UNDEFINED;
    std::vector<VkImage>      m_swapchainImages;
    std::vector<VkImageView>  m_swapchainImageViews;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    // --- Depth ---
    VkImage          m_depthImage        = VK_NULL_HANDLE;
    VkDeviceMemory   m_depthImageMemory  = VK_NULL_HANDLE;
    VkImageView      m_depthImageView    = VK_NULL_HANDLE;

    // --- MSAA ---
    VkSampleCountFlagBits m_msaaSamples  = VK_SAMPLE_COUNT_1_BIT; // MSAA not yet implemented

    // --- Render pass ---
    VkRenderPass     m_renderPass        = VK_NULL_HANDLE;

    // --- Command pool ---
    VkCommandPool    m_commandPool       = VK_NULL_HANDLE;

    // --- Sync ---
    // Semaphores and fences are indexed by swapchain image index.
    // Each swapchain image has its own acquire semaphore, render-finished
    // semaphore, and fence.  This ensures a semaphore is never reused
    // while the presentation engine still holds it.
    // On first frame, all fences are created signaled so the initial
    // wait passes immediately.
    //
    // In beginFrame we acquire with semaphore[oldImage], then store
    // oldImage in m_acquireImageIdx so submitFrame can wait on the
    // correct acquire semaphore (the one the presentation engine
    // signaled, which corresponds to the old image, not the new one).
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;
    uint32_t                 m_currentSwapchainImage = 0;
    uint32_t                 m_acquireImageIdx      = 0;

    // --- Internal helpers ---
    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSurface(GLFWwindow* window);
    void createSwapchain();
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createSyncObjects();

    void cleanupSwapchain();

    bool isDeviceSuitable(VkPhysicalDevice dev) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice dev) const;
    VkSampleCountFlagBits getMaxSampleCount(VkPhysicalDevice physDev) const;

    // Validation layers / extensions
    std::vector<const char*> getRequiredExtensions() const;
    bool checkValidationLayerSupport() const;
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT*,
        void*);
};
