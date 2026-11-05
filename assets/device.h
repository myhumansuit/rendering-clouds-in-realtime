#ifndef DEVICE_H
#define DEVICE_H
 
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
 
// Result of physical device selection.
typedef struct {
    VkPhysicalDevice physical;
    uint32_t queue_family;              // family that does graphics + compute + present
    VkPhysicalDeviceProperties props;   // name, type, API version, limits
} GpuChoice;
 
// Human-readable name for a device type ("discrete GPU", "CPU (software)", ...).
const char *device_type_name(VkPhysicalDeviceType t);
 
// Lists every GPU (printing its name, type and Vulkan version), rejects unsuitable
// ones, and fills *out with the best of the rest. A device is suitable if it has:
//   - Vulkan 1.3 or later
//   - the VK_KHR_swapchain device extension
//   - the dynamicRendering and synchronization2 features
//   - a queue family supporting graphics, compute and presenting to `surface`
// Preference order: discrete > integrated > virtual > CPU.
// Returns false (and leaves *out untouched) if nothing is suitable.
bool pick_physical_device(VkInstance instance, VkSurfaceKHR surface, GpuChoice *out);
 
// Creates the logical device from a GpuChoice, enabling the swapchain extension and
// the dynamicRendering and synchronization2 features, and fetches its single queue.
// The caller owns *device and must call vkDestroyDevice on it. The queue is owned by
// the device and is never destroyed separately.
bool create_logical_device(const GpuChoice *gpu, VkDevice *device, VkQueue *queue);
 
#endif