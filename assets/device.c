#include "device.h"

const char *device_type_name(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU (software)";
        default:                                     return "other";
    }
}

static bool has_device_extension(VkPhysicalDevice dev, const char *name) {
    //Count the number of extensions
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, NULL, &count, NULL);
    if (count == 0) return false;
    
    //Fill array with extensions
    VkExtensionProperties *exts = malloc(count * sizeof *exts);
    if (!exts) return false;
    vkEnumerateDeviceExtensionProperties(dev, NULL, &count, exts);
 
    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, name) == 0) { found = true; break; }
    }
    free(exts);
    return found;
}

// Find a queue family that can do graphics AND compute AND present to the surface.
static bool find_queue_family(VkPhysicalDevice dev, VkSurfaceKHR surface,
                              uint32_t *out_family) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, NULL);
    if (count == 0) return false;

    VkQueueFamilyProperties *fams = malloc(count * sizeof *fams);
    if (!fams) return false;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, fams);

    const VkQueueFlags needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    bool found = false;
    for (uint32_t i = 0; i < count && !found; i++) {
        if ((fams[i].queueFlags & needed) != needed) continue;

        VkBool32 can_present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &can_present);
        if (can_present) {
            *out_family = i;
            found = true;
        }
    }
    free(fams);
    return found;
}

// Rank device types, never prefers the CPU (llvmpipe) over a real GPU.
static int score_device_type(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 4;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 2;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return 1;
        default:                                     return 0;
    }
}

bool pick_physical_device(VkInstance instance, VkSurfaceKHR surface, GpuChoice *out) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (count == 0) {
        fprintf(stderr, "No Vulkan devices found\n");
        return false;
    }

    VkPhysicalDevice *devs = malloc(count * sizeof *devs);
    if (!devs) return false;
    vkEnumeratePhysicalDevices(instance, &count, devs);

    int best_score = -1;

    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devs[i], &props);
        printf("GPU %u: %s, %s, Vulkan %u.%u.%u\n", i, props.deviceName,
               device_type_name(props.deviceType),
               VK_API_VERSION_MAJOR(props.apiVersion),
               VK_API_VERSION_MINOR(props.apiVersion),
               VK_API_VERSION_PATCH(props.apiVersion));

        // 1. Vulkan version
        if (props.apiVersion < VK_API_VERSION_1_3) {
            printf("  rejected: needs Vulkan 1.3\n");
            continue;
        }

        // 2. Swapchain extension
        if (!has_device_extension(devs[i], VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            printf("  rejected: no %s\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            continue;
        }

        // 3. Features
        VkPhysicalDeviceVulkan13Features f13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        };
        VkPhysicalDeviceFeatures2 f2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &f13,
        };
        vkGetPhysicalDeviceFeatures2(devs[i], &f2);
        if (!f13.dynamicRendering || !f13.synchronization2) {
            printf("  rejected: needs dynamicRendering and synchronization2\n");
            continue;
        }

        // 4. Queue family for graphics + compute + present
        uint32_t family;
        if (!find_queue_family(devs[i], surface, &family)) {
            printf("  rejected: no graphics+compute+present queue family\n");
            continue;
        }

        // Keep it if it beats the best so far.
        int score = score_device_type(props.deviceType);
        if (score > best_score) {
            best_score = score;
            out->physical = devs[i];
            out->queue_family = family;
            out->props = props;
        }
    }

    free(devs);
    return best_score >= 0;
}

bool create_logical_device(const GpuChoice *gpu, VkDevice *device, VkQueue *queue) {
    // Which queues we want. The priority array must outlive vkCreateDevice.
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = gpu->queue_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    // Features to turn on
    VkPhysicalDeviceVulkan13Features f13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .dynamicRendering = VK_TRUE,
        .synchronization2 = VK_TRUE,
    };

    const char *extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &f13,                        // feature chain
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = sizeof extensions / sizeof extensions[0],
        .ppEnabledExtensionNames = extensions,
    };

    VkResult r = vkCreateDevice(gpu->physical, &device_info, NULL, device);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "vkCreateDevice failed: %d\n", r);
        return false;
    }

    vkGetDeviceQueue(*device, gpu->queue_family, 0, queue);
    return true;
}
