#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "device.h"

typedef struct {
    VkSwapchainKHR handle;
    VkExtent2D extent;
    VkFormat format;
    uint32_t image_count;
    VkImage *images;
    VkImageView *views;
}Swapchain;

// Creates a swapchain using all the given parameters and puts result in out
bool create_swapchain (VkDevice device, const GpuChoice * gpu, VkSurfaceKHR surface, SDL_Window *window, Swapchain *out);

//Destroy a swapchain
void destroy_swapchain(VkDevice device, Swapchain *sc);
#endif