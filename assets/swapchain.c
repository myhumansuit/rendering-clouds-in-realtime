#include "swapchain.h"
#include <vulkan/vulkan_core.h>

//Loop through formats and if available, pick 32 bit srgb normalized values
static VkSurfaceFormatKHR choose_format(const VkSurfaceFormatKHR *formats, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[i];
        }
    }
    return formats[0];
}


bool create_swapchain(VkDevice device, const GpuChoice *gpu, VkSurfaceKHR surface, SDL_Window *window, Swapchain *out) {
    // Get surface capabilities and details
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->physical, surface,  &capabilities);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: %d\n", r);
        return false;
    }

    printf("images %u-%u, extent %ux%u, usage 0x%x\n", capabilities.minImageCount, capabilities.maxImageCount,
       capabilities.currentExtent.width, capabilities.currentExtent.height, capabilities.supportedUsageFlags);
    
    // Get formats and choose one
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physical, surface, &format_count, NULL);
    if (format_count == 0) {
        fprintf(stderr, "Surface reports no formats\n");
        return false;
    }

    VkSurfaceFormatKHR *formats = malloc(format_count * sizeof *formats);
    if (!formats) return false;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physical, surface, &format_count, formats);
    VkSurfaceFormatKHR chosen_format = choose_format(formats, format_count);
    free(formats);

    for (uint32_t i = 0; i < format_count; i++)
        printf("format %d, colour space %d\n", (int)formats[i].format, (int)formats[i].colorSpace);

    
    return true;
}
