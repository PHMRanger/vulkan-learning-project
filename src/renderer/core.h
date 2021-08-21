#ifndef RENDERER_CORE_H
#define RENDERER_CORE_H

#include <vulkan/vulkan.h>

#ifdef DEBUG
    const uint8_t VULKAN_VALIDATE = 1;
#else
    const uint8_t VULKAN_VALIDATE = 0;
#endif

const char* const VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};

const char* const DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const uint32_t FRAMES_IN_FLIGHT = 2;

typedef enum RenderResult {
    RENDER_SUCCESS = 0,
    RENDER_FAIL = 1,
    RENDER_NOFILE = -5
} RenderResult;

void check_vulkan_result(VkResult &result, char *msg)
{
    if (result != VK_SUCCESS) {
        fmt::print("assert failed: {}", msg);
    }
}

struct QueueFamilyIndices {
    uint32_t graphics;
    uint32_t present;

    bool has_graphics;
    bool has_present;
};

struct CommandPool {
    VkCommandPool pool;
    VkCommandBuffer *buffers;
};

struct Renderer {
    GLFWwindow *window;
    VkSurfaceKHR surface;

    VkInstance instance;
    VkPhysicalDevice physical_device;

    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;

    uint32_t image_count;
    VkImage *images;
    VkImageView *views;
    VkFramebuffer *framebuffers;

    VkRenderPass pass;
    CommandPool cmdpools[FRAMES_IN_FLIGHT];

    uint32_t current_frame = 0;
    uint32_t frame_index = 0;
    VkSemaphore *available;
    VkSemaphore *rendered;
    VkFence *inflight;
    VkFence *image_inflight;
};

#endif