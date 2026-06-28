#include "vulkan_context.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <algorithm>
#include <limits>
#include <fstream>

// -----------------------------------------------------------------------------
// Validation layers
// -----------------------------------------------------------------------------
static const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// -----------------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------------
VulkanContext::VulkanContext() {}
VulkanContext::~VulkanContext() {
    if (m_device) {
        vkDeviceWaitIdle(m_device);
    }
}

// -----------------------------------------------------------------------------
// init
// -----------------------------------------------------------------------------
void VulkanContext::init(GLFWwindow* window) {
    m_window = window;

    createInstance();
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createSyncObjects(); // after swapchain so we know image count
}

// -----------------------------------------------------------------------------
// cleanup
// -----------------------------------------------------------------------------
void VulkanContext::cleanup() {
    if (!m_device) return;

    vkDeviceWaitIdle(m_device);

    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    cleanupSwapchain();

    vkDestroyDevice(m_device, nullptr);

    if (m_enableValidation && m_debugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_instance, m_debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

// -----------------------------------------------------------------------------
// Frame management
// -----------------------------------------------------------------------------
bool VulkanContext::beginFrame() {
    // The acquire semaphore is indexed by the *outgoing* swapchain image
    // (the one we last acquired and submitted work for).  Save it before
    // m_currentSwapchainImage changes.
    m_acquireImageIdx = m_currentSwapchainImage;

    // Wait for the GPU to finish using the outgoing image.
    // All fences are created signaled, so the very first call passes.
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_acquireImageIdx],
                    VK_TRUE, UINT64_MAX);

    // Acquire the next swapchain image.
    // m_imageAvailableSemaphores[m_acquireImageIdx] is unsignaled
    // (consumed by vkQueueSubmit the last time this image was rendered
    // to).  vkAcquireNextImageKHR will signal it when the presentation
    // engine releases the outgoing image.
    VkResult result = vkAcquireNextImageKHR(
        m_device, m_swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_acquireImageIdx],
        VK_NULL_HANDLE, &m_currentSwapchainImage);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return false;
    }
    assert(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);

    // Reset the fence for the image we just acquired.  It was signaled
    // from the wait above (signaled from previous render to this image)
    // or signaled from creation (first use).  Either way, resetting is
    // safe — we want it unsignaled so vkQueueSubmit can re-signal it.
    VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentSwapchainImage]));

    return true;
}

void VulkanContext::submitFrame(VkCommandBuffer cmd) {
    uint32_t img = m_currentSwapchainImage;

    // Wait on the acquire semaphore that was passed to
    // vkAcquireNextImageKHR in beginFrame.  That semaphore corresponds
    // to the *outgoing* image (m_acquireImageIdx).  The presentation
    // engine signaled it when that image was released for reuse.
    // We wait here before writing to the new image's color attachment.
    //
    // Signal the render-finished semaphore for the NEW image so that
    // vkQueuePresentKHR waits for rendering to complete.
    // Signal the per-image fence for the NEW image so beginFrame can
    // wait on it next time this image is acquired.
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = &m_imageAvailableSemaphores[m_acquireImageIdx];
    submitInfo.pWaitDstStageMask  = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores  = &m_renderFinishedSemaphores[img];

    VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[img]));

    // Present only after rendering completes
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphores[img];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &img;

    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }
}

// -----------------------------------------------------------------------------
// Memory helper
// -----------------------------------------------------------------------------
uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    assert(false && "Failed to find suitable memory type");
    return UINT32_MAX;
}

// -----------------------------------------------------------------------------
// Single-time commands
// -----------------------------------------------------------------------------
VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

// -----------------------------------------------------------------------------
// Instance
// -----------------------------------------------------------------------------
void VulkanContext::createInstance() {
    if (m_enableValidation && !checkValidationLayerSupport()) {
        std::cerr << "Validation layers requested but not available.\n";
        m_enableValidation = false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "a-voxel-engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_enableValidation) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

        debugCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        debugCreateInfo.pUserData       = this;
        createInfo.pNext                = &debugCreateInfo;
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));

    std::cout << "Vulkan instance created.\n";
}

// -----------------------------------------------------------------------------
// Debug messenger
// -----------------------------------------------------------------------------
void VulkanContext::setupDebugMessenger() {
    if (!m_enableValidation) return;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (!func) {
        std::cerr << "vkCreateDebugUtilsMessengerEXT not available.\n";
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    info.pUserData       = this;

    func(m_instance, &info, nullptr, &m_debugMessenger);
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Vulkan] " << data->pMessage << "\n";
    }
    return VK_FALSE;
}

// -----------------------------------------------------------------------------
// Surface
// -----------------------------------------------------------------------------
void VulkanContext::createSurface(GLFWwindow* window) {
    VK_CHECK(glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface));
}

// -----------------------------------------------------------------------------
// Physical device
// -----------------------------------------------------------------------------
void VulkanContext::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    assert(count > 0);

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (const auto& dev : devices) {
        if (isDeviceSuitable(dev)) {
            m_physicalDevice = dev;
            // m_msaaSamples    = getMaxSampleCount(dev); // disabled — MSAA not implemented
            break;
        }
    }

    assert(m_physicalDevice != VK_NULL_HANDLE);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    std::cout << "Physical device: " << props.deviceName << "\n";
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice dev) const {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(dev, &props);

    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        // Prefer discrete GPUs
    }

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(dev, &features);

    if (!features.samplerAnisotropy) return false;

    // Check queue families
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, queues.data());

    bool graphics = false, present = false;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics = true;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &presentSupport);
        if (presentSupport) present = true;
    }
    if (!graphics || !present) return false;

    // Extensions
    if (!checkDeviceExtensionSupport(dev)) return false;

    // Swapchain adequacy
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &fmtCount, nullptr);
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface, &pmCount, nullptr);
    if (fmtCount == 0 || pmCount == 0) return false;

    return true;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice dev) const {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, available.data());

    std::set<std::string> required(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    for (const auto& ext : available) {
        required.erase(ext.extensionName);
    }
    return required.empty();
}

VkSampleCountFlagBits VulkanContext::getMaxSampleCount(VkPhysicalDevice dev) const {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(dev, &props);
    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                 props.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

// -----------------------------------------------------------------------------
// Logical device
// -----------------------------------------------------------------------------
void VulkanContext::createLogicalDevice() {
    // Find queue families
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qCount, queues.data());

    uint32_t graphics = UINT32_MAX, present = UINT32_MAX;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && graphics == UINT32_MAX) {
            graphics = i;
            VkBool32 support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &support);
            if (support) present = i; // use same if possible
        }
    }
    // If present wasn't set yet, find a dedicated present queue
    if (present == UINT32_MAX) {
        for (uint32_t i = 0; i < qCount; ++i) {
            VkBool32 support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &support);
            if (support) { present = i; break; }
        }
    }

    assert(graphics != UINT32_MAX && present != UINT32_MAX);
    m_graphicsFamily = graphics;
    m_presentFamily  = present;

    std::set<uint32_t> uniqueFamilies = { graphics, present };
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float priority = 1.0f;

    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo info{};
        info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = family;
        info.queueCount       = 1;
        info.pQueuePriorities = &priority;
        queueInfos.push_back(info);
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid  = VK_TRUE; // for wireframe if desired

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.pEnabledFeatures        = &features;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

    if (m_enableValidation) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, graphics, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, present, 0, &m_presentQueue);
}

// -----------------------------------------------------------------------------
// Swapchain
// -----------------------------------------------------------------------------
void VulkanContext::createSwapchain() {
    // Choose format, present mode, extent
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, formats.data());

    // Pick format: prefer BGRA8 SRGB
    m_surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_surfaceFormat = f;
            break;
        }
    }
    m_swapchainFormat = m_surfaceFormat.format;

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &pmCount, presentModes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed available
    for (const auto& pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = pm;
            break;
        }
    }

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);

    // Extent
    if (caps.currentExtent.width != UINT32_MAX) {
        m_swapchainExtent = caps.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        m_swapchainExtent = {
            std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = m_surface;
    info.minImageCount    = imageCount;
    info.imageFormat      = m_surfaceFormat.format;
    info.imageColorSpace  = m_surfaceFormat.colorSpace;
    info.imageExtent      = m_swapchainExtent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t families[] = { m_graphicsFamily, m_presentFamily };
    if (m_graphicsFamily != m_presentFamily) {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = families;
    } else {
        info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform   = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = presentMode;
    info.clipped        = VK_TRUE;
    info.oldSwapchain   = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain));

    // Retrieve images
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
}

void VulkanContext::createImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image      = m_swapchainImages[i];
        info.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        info.format     = m_swapchainFormat;
        info.components = { VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY };
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount     = 1;

        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &m_swapchainImageViews[i]));
    }
}

// -----------------------------------------------------------------------------
// Depth resources
// -----------------------------------------------------------------------------
void VulkanContext::createDepthResources() {
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    // Create depth image
    VkImageCreateInfo imgInfo{};
    imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.extent.width  = m_swapchainExtent.width;
    imgInfo.extent.height = m_swapchainExtent.height;
    imgInfo.extent.depth  = 1;
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.format        = depthFormat;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.samples       = m_msaaSamples;

    VK_CHECK(vkCreateImage(m_device, &imgInfo, nullptr, &m_depthImage));

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device, m_depthImage, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthImageMemory));

    vkBindImageMemory(m_device, m_depthImage, m_depthImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image      = m_depthImage;
    viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format     = depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView));
}

// -----------------------------------------------------------------------------
// Render pass
// -----------------------------------------------------------------------------
void VulkanContext::createRenderPass() {
    bool multisampled = m_msaaSamples != VK_SAMPLE_COUNT_1_BIT;

    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_swapchainFormat;
    colorAttachment.samples        = multisampled ? m_msaaSamples : VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = multisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = multisampled ? m_msaaSamples : VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription resolveAttachment{};
    VkAttachmentReference resolveRef{};
    std::vector<VkAttachmentDescription> attachments;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    if (multisampled) {
        resolveAttachment.format         = m_swapchainFormat;
        resolveAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        resolveRef.attachment = 2;
        resolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        subpass.pResolveAttachments = &resolveRef;

        // Adjust depth attachment index
        depthRef.attachment = 1;

        attachments = { colorAttachment, depthAttachment, resolveAttachment };
    } else {
        attachments = { colorAttachment, depthAttachment };
    }

    // External subpass dependency: wait at the top of the pipeline for the
    // previous frame's rendering to complete (the semaphore in vkQueueSubmit
    // already guarantees this), then block until the image layout transition
    // (VK_IMAGE_LAYOUT_UNDEFINED → COLOR_ATTACHMENT_OPTIMAL + depth) finishes
    // before we write to color/depth attachments.
    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<uint32_t>(attachments.size());
    info.pAttachments    = attachments.data();
    info.subpassCount    = 1;
    info.pSubpasses      = &subpass;
    info.dependencyCount = 1;
    info.pDependencies   = &dep;

    VK_CHECK(vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass));
}

// -----------------------------------------------------------------------------
// Framebuffers
// -----------------------------------------------------------------------------
void VulkanContext::createFramebuffers() {
    // With MSAA + resolve, we use separate color/depth images per swapchain image.
    // For simplicity, we create the framebuffers referencing swapchain image views
    // and a shared depth image. With MSAA we also need per-swapchain MSAA color images.
    // To keep this initial implementation simple, let's disable MSAA for now (set to 1 bit).
    // We'll enable it once the basic pipeline works.
    m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {
            m_swapchainImageViews[i],
            m_depthImageView
        };

        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = m_renderPass;
        info.attachmentCount = static_cast<uint32_t>(attachments.size());
        info.pAttachments    = attachments.data();
        info.width           = m_swapchainExtent.width;
        info.height          = m_swapchainExtent.height;
        info.layers          = 1;

        VK_CHECK(vkCreateFramebuffer(m_device, &info, nullptr, &m_swapchainFramebuffers[i]));
    }
}

// -----------------------------------------------------------------------------
// Command pool
// -----------------------------------------------------------------------------
void VulkanContext::createCommandPool() {
    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = m_graphicsFamily;

    VK_CHECK(vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool));
}

// -----------------------------------------------------------------------------
// Sync objects
// -----------------------------------------------------------------------------
void VulkanContext::createSyncObjects() {
    // Create one semaphore pair and one fence per swapchain image.
    // Each image has its own acquire/render semaphores and fence,
    // preventing "semaphore still in use" validation warnings.
    uint32_t count = imageCount();
    m_imageAvailableSemaphores.resize(count);
    m_renderFinishedSemaphores.resize(count);
    m_inFlightFences.resize(count);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < count; ++i) {
        VK_CHECK(vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]));
    }
}

// -----------------------------------------------------------------------------
// Cleanup / recreate swapchain
// -----------------------------------------------------------------------------
void VulkanContext::cleanupSwapchain() {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);

    for (auto fb : m_swapchainFramebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);

    for (auto iv : m_swapchainImageViews)
        vkDestroyImageView(m_device, iv, nullptr);

    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

void VulkanContext::recreateSwapchain() {
    int w = 0, h = 0;
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(m_window, &w, &h);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device);

    // Destroy old sync objects (sized by the old image count)
    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();

    cleanupSwapchain();

    createSwapchain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
    createSyncObjects();  // re-sized and re-created for new imageCount()

    // Reset image indices to safe defaults — the old values may be >= new count
    m_currentSwapchainImage = 0;
    m_acquireImageIdx      = 0;
}

// -----------------------------------------------------------------------------
// Extension / validation layer helpers
// -----------------------------------------------------------------------------
std::vector<const char*> VulkanContext::getRequiredExtensions() const {
    uint32_t count;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&count);

    std::vector<const char*> exts(glfwExts, glfwExts + count);
    if (m_enableValidation) {
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return exts;
}

bool VulkanContext::checkValidationLayerSupport() const {
    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());

    for (const auto& name : VALIDATION_LAYERS) {
        bool found = false;
        for (const auto& layer : layers) {
            if (strcmp(name, layer.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}
