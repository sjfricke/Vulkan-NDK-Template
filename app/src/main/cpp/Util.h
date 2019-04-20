#ifndef VULKAN_UTIL_H_
#define VULKAN_UTIL_H_

#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>

// used to get logcat outputs which can be regex filtered by the LOG_TAG we give
// So in Logcat you can filter this example by putting "VULKAN"
#define LOG_TAG "VULKAN"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ASSERT(cond, fmt, ...)                                    \
    if (!(cond)) {                                                \
        __android_log_assert(#cond, LOG_TAG, fmt, ##__VA_ARGS__); \
    }

// Vulkan call wrapper
#define CALL_VK(func)                                                                                              \
    if (VK_SUCCESS != (func)) {                                                                                    \
        __android_log_print(ANDROID_LOG_ERROR, "Vulkan ", "Vulkan error. File[%s], line[%d]", __FILE__, __LINE__); \
        assert(false);                                                                                             \
    }

// A macro to check value is VK_SUCCESS
// Used also for non-vulkan functions but return VK_SUCCESS
#define VK_CHECK(x) CALL_VK(x)

#endif  // VULKAN_UTIL_H_
