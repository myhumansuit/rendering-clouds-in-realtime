#include "swapchain.h"
#include <vulkan/vulkan_core.h>


static uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}


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

    printf("images %u-%u, extent %xx%x, usage 0x%x\n", capabilities.minImageCount, capabilities.maxImageCount,
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

    for (uint32_t i = 0; i < format_count; i++)
        printf("format %d, colour space %d\n", (int)formats[i].format, (int)formats[i].colorSpace);
    free(formats);
    
    VkExtent2D extent = capabilities.currentExtent;
    // 0xFFFFFFFF = Any size allowed, so get from the SDL window
    if (extent.width == 0xFFFFFFFF) {
        int w = 0, h = 0;
        if (!SDL_GetWindowSizeInPixels(window, &w, &h)) {
            fprintf(stderr, "SDL_GetWindowSizeInPixels failed: %s\n", SDL_GetError());
            return false;
        }
        extent.width  = clamp_u32((uint32_t)w, capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
        extent.height = clamp_u32((uint32_t)h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    // Ask for 1 more than the minimum number of images, bounded by max image count (value of 0 means no max)
    uint32_t min_images = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && min_images > capabilities.maxImageCount) {
        min_images = capabilities.maxImageCount;
    }

    // Make sure we have the features we want
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((capabilities.supportedUsageFlags & usage) != usage) {
        fprintf(stderr, "c.suc.suSurface does not support the requested image usage\n");
        return false;
    }
    if (!(capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
        fprintf(stderr, "Surface does not support opaque composite alpha\n");
        return false;
    }

    // Info needed for the swapchain
    VkSwapchainCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = min_images,
        .imageFormat = chosen_format.format,
        .imageColorSpace = chosen_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = usage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,  // one queue family
        .preTransform = capabilities.currentTransform,  // don't rotate
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,    // vsync; guaranteed to exist
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE, // set this when recreating on resize
    };

    // Create swapchain
    r = vkCreateSwapchainKHR(device, &info, NULL, &out->handle);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "vkCreateSwapchainKHR failed: %d\n", r);
        return false;
    }
    out->format = chosen_format.format;
    out->extent = extent;
 
    // Obtain swapchain images, driver may give more than we asked for
    vkGetSwapchainImagesKHR(device, out->handle, &out->image_count, NULL);
    out->images = malloc(out->image_count * sizeof *out->images);
    out->views  = calloc(out->image_count, sizeof *out->views);   // zeroed so cleanup is safe

    // If we don't have images/views abort
    if (!out->images || !out->views) {
        destroy_swapchain(device, out);
        return false;
    }
    vkGetSwapchainImagesKHR(device, out->handle, &out->image_count, out->images);

    // One view per image
    for (uint32_t i = 0; i < out->image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = out->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = out->format,
            .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
            },
        };
        r = vkCreateImageView(device, &view_info, NULL, &out->views[i]);
        if (r != VK_SUCCESS) {
            fprintf(stderr, "vkCreateImageView failed: %d\n", r);
            destroy_swapchain(device, out);
            return false;
        }
    }
 
    printf("Swapchain: %u images, %ux%u, format %d, colour space %d, FIFO\n",
           out->image_count, extent.width, extent.height,
           (int)chosen_format.format, (int)chosen_format.colorSpace);
    
    return true;
}

void destroy_swapchain(VkDevice device, Swapchain *sc) {

    // If there are views, destroy them
    if (sc->views) {
        for (uint32_t i = 0; i < sc->image_count; i++) {
            if (sc->views[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(device, sc->views[i], NULL);
            }
        }
    }

    // Free view and image arrays
    free(sc->views);
    free(sc->images);
    if (sc->handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, sc->handle, NULL);
    }
    memset(sc, 0, sizeof *sc);
    printf("Swapchain successfully destroyed\n");
}

