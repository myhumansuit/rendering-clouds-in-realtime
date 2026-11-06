#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <SDL3/SDL_stdinc.h>

#include "device.h"
#include "swapchain.h"

#define CHECK(r) do {\
    VkResult _r = (r); \
    if (_r != VK_SUCCESS) { \
        fprintf(stderr, "%s:%d failed check: %d\n", __FILE__, __LINE__, _r); \
        return 1; \
    } \
}while (0)


int main (void) {
    
    //initialise sdl
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    //create sdl window, loading vulkan as a side-effect
    SDL_Window *window = SDL_CreateWindow("test", 1280, 720,
                                          SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    //Find the extensions SDL Needs for windows
    Uint32 sdl_extension_count = 0;
    const char * const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);

    //Make a copy of the extensions, adding debug utils to the end
    const char **extensions = malloc((sdl_extension_count + 1) * sizeof(char *));
    memcpy(extensions, sdl_extensions, sdl_extension_count * sizeof(char *));
    extensions[sdl_extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    for (Uint32 i = 0; i < sdl_extension_count; i++) {
    printf("SDL wants: %s\n", sdl_extensions[i]);
    }

    //Which enabled layers we want in our vulkan instance
    const char *layers[] = {"VK_LAYER_KHRONOS_validation", //error checking, costs performance
                            "VK_LAYER_MESA_overlay" };

    //Setup structs for vkinstance creation
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "cloud renderer",
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = sizeof(layers) / sizeof(layers[0]),
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = sdl_extension_count + 1,
        .ppEnabledExtensionNames = extensions,
    };

    //Create vkinstance into instance and check that it worked
    VkInstance instance;
    VkResult r = vkCreateInstance(&create_info, NULL, &instance);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "vkCreateInstance failed: %d\n",r);
        return 1;
    }

    //Create a surface from the window to give to vulkan
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, NULL, &surface)){
        fprintf(stderr, "SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return 1;
    }

    GpuChoice gpu;
    
    if (!pick_physical_device(instance, surface, &gpu)) {
        fprintf(stderr, "No suitable GPU\n");
        return 1;
    }
    
    printf("Selected: %s (queue family %u)\n", gpu.props.deviceName, gpu.queue_family);

    VkDevice device;
    VkQueue queue;
    if (!create_logical_device(&gpu, &device, &queue)) {
        fprintf(stderr, "Logical Device Creation Failed :(");
        return 1;
    }

    Swapchain swapchain;
    create_swapchain(device, &gpu, surface, window, &swapchain);

    //Frame loop
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
        }
        SDL_Delay(10);
    }




    //Destroy all instances/windows we have set up

    vkDestroyDevice(device, NULL);
    SDL_Vulkan_DestroySurface(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    SDL_DestroyWindow(window);
    SDL_Quit();
}