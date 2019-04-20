#ifndef VULKAN_VULKANMAIN_H_
#define VULKAN_VULKANMAIN_H_

#include <android_native_app_glue.h>
#include "Util.h"

#define VULKAN_VALIDATION_LAYERS true

bool InitVulkanContext(android_app* app);

void DeleteVulkanContext(void);

bool IsVulkanReady(void);

bool VulkanDrawFrame(android_app* app);

#endif  // VULKAN_VULKANMAIN_H_
