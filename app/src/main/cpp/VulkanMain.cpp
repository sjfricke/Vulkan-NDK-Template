#include "VulkanMain.h"
#include <android/log.h>
#include <malloc.h>
#include <cassert>
#include <vector>
#include "CreateShaderModule.h"
#include "ValidationLayers.h"
#include "vulkan_wrapper.h"

// Global Variables ...
struct VulkanDeviceInfo {
    bool initialized;
    VkInstance instance;
    VkPhysicalDevice gpuDevice;
    VkPhysicalDeviceMemoryProperties gpuMemoryProperties;
    VkDevice device;
    VkSurfaceKHR surface;
    VkQueue queue;
    uint32_t queueFamilyIndex;
};
VulkanDeviceInfo device;

struct VulkanSwapchainInfo {
    VkSwapchainKHR swapchain;
    uint32_t swapchainLength;
    VkExtent2D displaySize;
    VkFormat displayFormat;
    VkFramebuffer* framebuffers;  // array of frame buffers and views
    VkImageView* imageViews;
};
VulkanSwapchainInfo swapchain;

typedef struct texture_object {
    VkSampler sampler;
    VkImage image;
    VkImageLayout imageLayout;
    VkDeviceMemory memory;
    VkImageView view;
    int32_t texWidth, texHeight;
} texture_object;

struct VulkanBufferInfo {
    VkBuffer vertexBuffer;
};
VulkanBufferInfo buffers;

struct VulkanGfxPipelineInfo {
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkPipelineLayout layout;
    VkPipelineCache cache;
    VkPipeline pipeline;
};
VulkanGfxPipelineInfo gfxPipeline;

struct VulkanRenderInfo {
    VkRenderPass renderPass;
    VkCommandPool cmdPool;
    VkCommandBuffer* cmdBuffer;
    uint32_t cmdBufferLength;
    VkSemaphore semaphore;
    VkFence fence;
};
VulkanRenderInfo render;

void* mappedData;

VkDebugReportCallbackEXT debugCallbackHandle;

// Android Native App pointer...
android_app* androidAppCtx = nullptr;

// Create vulkan device
void CreateVulkanDevice(ANativeWindow* platformWindow, VkApplicationInfo* appInfo) {
    std::vector<const char*> instanceExtensions;
    std::vector<const char*> instanceLayers;
    std::vector<const char*> deviceExtensions;

    instanceExtensions.push_back("VK_KHR_surface");
    instanceExtensions.push_back("VK_KHR_android_surface");
    deviceExtensions.push_back("VK_KHR_swapchain");

#if (VULKAN_VALIDATION_LAYERS)
    instanceExtensions.push_back("VK_EXT_debug_report");
    // VK_GOOGLE_THREADING_LAYER better to be the very first one
    // VK_LAYER_GOOGLE_unique_objects need to be after VK_LAYER_LUNARG_core_validation
    instanceLayers.push_back("VK_LAYER_GOOGLE_threading");
    instanceLayers.push_back("VK_LAYER_LUNARG_core_validation");
    instanceLayers.push_back("VK_LAYER_GOOGLE_unique_objects");
    instanceLayers.push_back("VK_LAYER_LUNARG_object_tracker");
    instanceLayers.push_back("VK_LAYER_LUNARG_parameter_validation");
#endif

    // Create the Vulkan instance
    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
    };
    CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &device.instance));

#if (VULKAN_VALIDATION_LAYERS)
    CreateDebugReportExt(device.instance, &debugCallbackHandle);
#endif

    VkAndroidSurfaceCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                                             .pNext = nullptr,
                                             .flags = 0,
                                             .window = platformWindow};

    CALL_VK(vkCreateAndroidSurfaceKHR(device.instance, &createInfo, nullptr, &device.surface));

    uint32_t gpuCount = 0;
    CALL_VK(vkEnumeratePhysicalDevices(device.instance, &gpuCount, nullptr));
    VkPhysicalDevice tmpGpus[gpuCount];
    CALL_VK(vkEnumeratePhysicalDevices(device.instance, &gpuCount, tmpGpus));
    device.gpuDevice = tmpGpus[0];  // Pick up the first GPU Device

    // Find a Graphics queue family
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(device.gpuDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device.gpuDevice, &queueFamilyCount, queueFamilyProperties.data());

    uint32_t queueFamilyIndex;
    for (queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; queueFamilyIndex++) {
        if (queueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            break;
        }
    }
    assert(queueFamilyIndex < queueFamilyCount);
    device.queueFamilyIndex = queueFamilyIndex;

    vkGetPhysicalDeviceMemoryProperties(device.gpuDevice, &device.gpuMemoryProperties);

    // Create a logical device (vulkan device)
    float priorities[] = {
        1.0f,
    };
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCount = 1,
        .queueFamilyIndex = device.queueFamilyIndex,
        .pQueuePriorities = priorities,
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = nullptr,
    };

    CALL_VK(vkCreateDevice(device.gpuDevice, &deviceCreateInfo, nullptr, &device.device));
    vkGetDeviceQueue(device.device, device.queueFamilyIndex, 0, &device.queue);
}

void CreateSwapChain() {
    memset(&swapchain, 0, sizeof(swapchain));

    // Get the surface capabilities because:
    //   - It contains the minimal and max length of the chain, we will need it
    //   - It's necessary to query the supported surface format (R8G8B8A8 for instance ...)
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpuDevice, device.surface, &surfaceCapabilities);
    // Query the list of supported surface format and choose one we like
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice, device.surface, &formatCount, nullptr);
    VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[formatCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice, device.surface, &formatCount, formats);
    LOGI("Got %d formats", formatCount);

    uint32_t chosenFormat;
    for (chosenFormat = 0; chosenFormat < formatCount; chosenFormat++) {
        if (formats[chosenFormat].format == VK_FORMAT_R8G8B8A8_UNORM) break;
    }
    assert(chosenFormat < formatCount);

    swapchain.displaySize = surfaceCapabilities.currentExtent;
    swapchain.displayFormat = formats[chosenFormat].format;
    // Create a swap chain (here we choose the minimum available number of surface
    // in the chain)
    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = device.surface,
        .minImageCount = surfaceCapabilities.minImageCount,
        .imageFormat = formats[chosenFormat].format,
        .imageColorSpace = formats[chosenFormat].colorSpace,
        .imageExtent = surfaceCapabilities.currentExtent,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .imageArrayLayers = 1,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &device.queueFamilyIndex,
        .compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .oldSwapchain = VK_NULL_HANDLE,
        .clipped = VK_FALSE,
    };
    CALL_VK(vkCreateSwapchainKHR(device.device, &swapchainCreateInfo, nullptr, &swapchain.swapchain));

    // Get the length of the created swap chain
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &swapchain.swapchainLength, nullptr));
    delete[] formats;
}

void CreateFrameBuffers(VkRenderPass& renderPass, VkImageView depthView = VK_NULL_HANDLE) {
    // query display attachment to swapchain
    uint32_t SwapchainImagesCount = 0;
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &SwapchainImagesCount, nullptr));
    VkImage* displayImages = new VkImage[SwapchainImagesCount];
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &SwapchainImagesCount, displayImages));

    // create image view for each swapchain image
    swapchain.imageViews = new VkImageView[SwapchainImagesCount];
    for (uint32_t i = 0; i < SwapchainImagesCount; i++) {
        VkImageViewCreateInfo viewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .image = displayImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain.displayFormat,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_R,
                    .g = VK_COMPONENT_SWIZZLE_G,
                    .b = VK_COMPONENT_SWIZZLE_B,
                    .a = VK_COMPONENT_SWIZZLE_A,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .flags = 0,
        };
        CALL_VK(vkCreateImageView(device.device, &viewCreateInfo, nullptr, &swapchain.imageViews[i]));
    }
    delete[] displayImages;

    // create a framebuffer from each swapchain image
    swapchain.framebuffers = new VkFramebuffer[swapchain.swapchainLength];
    for (uint32_t i = 0; i < swapchain.swapchainLength; i++) {
        VkImageView attachments[2] = {
            swapchain.imageViews[i],
            depthView,
        };
        VkFramebufferCreateInfo fbCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .renderPass = renderPass,
            .layers = 1,
            .attachmentCount = 1,  // 2 if using depth
            .pAttachments = attachments,
            .width = static_cast<uint32_t>(swapchain.displaySize.width),
            .height = static_cast<uint32_t>(swapchain.displaySize.height),
        };
        fbCreateInfo.attachmentCount = (depthView == VK_NULL_HANDLE ? 1 : 2);

        CALL_VK(vkCreateFramebuffer(device.device, &fbCreateInfo, nullptr, &swapchain.framebuffers[i]));
    }
}

// A help function to map required memory property into a VK memory type
// memory type is an index into the array of 32 entries; or the bit index
// for the memory type ( each BIT of an 32 bit integer is a type ).
VkResult AllocateMemoryTypeFromProperties(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((device.gpuMemoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return VK_SUCCESS;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return VK_ERROR_MEMORY_MAP_FAILED;
}

// A helper function
bool MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(device.gpuDevice, &memoryProperties);
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

// Initialize Vulkan Context when android application window is created upon return, vulkan is ready to draw frames
bool InitVulkanContext(android_app* app) {
    androidAppCtx = app;

    if (InitVulkan() == false) {
        LOGW("Vulkan dlopen is unavailable, install vulkan and re-start");
        return false;
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .pApplicationName = "VULKAN",
        .pEngineName = "null",
    };

    // create a device
    CreateVulkanDevice(app->window, &appInfo);

    CreateSwapChain();

    device.initialized = true;
    return true;
}

// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady(void) { return device.initialized; }

void DeleteSwapChain() {
    for (int i = 0; i < swapchain.swapchainLength; i++) {
        vkDestroyFramebuffer(device.device, swapchain.framebuffers[i], nullptr);
        vkDestroyImageView(device.device, swapchain.imageViews[i], nullptr);
    }
    delete[] swapchain.framebuffers;
    delete[] swapchain.imageViews;

    vkDestroySwapchainKHR(device.device, swapchain.swapchain, nullptr);
}

void DeleteVulkanContext() {
    vkFreeCommandBuffers(device.device, render.cmdPool, render.cmdBufferLength, render.cmdBuffer);
    delete[] render.cmdBuffer;

    vkDestroyCommandPool(device.device, render.cmdPool, nullptr);
    vkDestroyRenderPass(device.device, render.renderPass, nullptr);
    DeleteSwapChain();

#if (VULKAN_VALIDATION_LAYERS)
    DestroyDebugReportExt(device.instance, debugCallbackHandle);
#endif

    vkDestroyDevice(device.device, nullptr);
    vkDestroyInstance(device.instance, nullptr);

    device.initialized = false;
}

bool VulkanDrawFrame(android_app* app) { return true; }
