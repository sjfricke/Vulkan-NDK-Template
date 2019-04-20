#ifndef VULKAN_CREATESHADERMODULE_H_
#define VULKAN_CREATESHADERMODULE_H_

#include <android_native_app_glue.h>
#include <vulkan_wrapper.h>

VkShaderModule LoadSPIRVShader(android_app* appInfo, const char* filePath, VkDevice vkDevice);

#endif  // VULKAN_CREATESHADERMODULE_H_
