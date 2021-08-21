#ifndef RENDERER_RENDERER_H
#define RENDERER_RENDERER_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fmt/core.h>
#include <linmath.h>

#include "core.h"
#include "buffers.h"
#include "material.h"
#include "camera.h"

static void create_instance(Renderer *renderer) 
{
    VkApplicationInfo app_info;
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Quack";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;

    if (VULKAN_VALIDATE) {
        create_info.enabledLayerCount = sizeof(VALIDATION_LAYERS) / sizeof(char *);
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    } else {
        create_info.enabledLayerCount = 0;
    }

    vkCreateInstance(&create_info, nullptr, &renderer->instance);
}

static void choose_physical(Renderer *renderer, QueueFamilyIndices *indicies)
{
    uint32_t device_count = 0;
    VkPhysicalDevice devices[32];

    vkEnumeratePhysicalDevices(renderer->instance, &device_count, nullptr);
    vkEnumeratePhysicalDevices(renderer->instance, &device_count, devices);

    for (size_t i = 0; i < device_count; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        fmt::print("{} {}\n", i, properties.deviceName);
    }

    renderer->physical_device = devices[0];

    uint32_t family_count = 0;
    VkQueueFamilyProperties properties[32];
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &family_count, nullptr);
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &family_count, properties);

    for (size_t i = 0; i < family_count; ++i) {
        if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indicies->has_graphics = true;
            indicies->graphics = i;
        }

        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(renderer->physical_device, i, renderer->surface, &present);

        if (present) {
            indicies->has_present = true;
            indicies->present = i;
        }

        if (indicies->has_graphics && indicies->has_present) {
            break;
        }
    }
}

static void create_logical(Renderer *renderer, QueueFamilyIndices *indicies) 
{
    uint32_t queue_count;
    uint32_t queue_indicies[2];
    VkDeviceQueueCreateInfo queue_infos[2];

    if (indicies->graphics == indicies->present) {
        queue_count = 1;
        queue_indicies[0] = indicies->graphics;
    } else {
        queue_count = 2;
        queue_indicies[0] = indicies->graphics;
        queue_indicies[1] = indicies->present;
    }

    float priority = 1.0f;
    for (size_t i = 0; i < queue_count; ++i) {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_indicies[i];
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &priority;

        queue_infos[i] = queue_create_info;
    }

    VkPhysicalDeviceFeatures features = {};
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_infos;
    create_info.queueCreateInfoCount = queue_count;
    create_info.pEnabledFeatures = &features;
    create_info.enabledExtensionCount = sizeof(DEVICE_EXTENSIONS) / sizeof(char *);
    create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;

    if (VULKAN_VALIDATE) {
        create_info.enabledLayerCount = sizeof(VALIDATION_LAYERS) / sizeof(char *);
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    } else {
        create_info.enabledLayerCount = 0;
    }

    vkCreateDevice(renderer->physical_device, &create_info, nullptr, &renderer->device);

    vkGetDeviceQueue(renderer->device, indicies->graphics, 0, &renderer->graphics_queue);
    vkGetDeviceQueue(renderer->device, indicies->present, 0, &renderer->present_queue);
}

static void create_swapchain(Renderer *renderer, QueueFamilyIndices *indicies) 
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physical_device, renderer->surface, &capabilities);

    uint32_t create_image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && create_image_count > capabilities.maxImageCount) {
        create_image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = renderer->surface;
    create_info.minImageCount = create_image_count;
    create_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = VkExtent2D{WIDTH, HEIGHT};
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t indicies_list[] = { indicies->graphics, indicies->present };

    if (indicies->graphics != indicies->present) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = indicies_list;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    vkCreateSwapchainKHR(renderer->device, &create_info, nullptr, &renderer->swapchain);

    vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain, &renderer->image_count, nullptr);
    renderer->images = (VkImage *)malloc(sizeof(VkImage) * (renderer->image_count));
    vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain, &renderer->image_count, renderer->images);
}

static void create_imageviews(Renderer *renderer)
{
    renderer->views = (VkImageView *)malloc(sizeof(VkImageView) * renderer->image_count);

    for (size_t i = 0; i < renderer->image_count; ++i) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = renderer->images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(renderer->device, &createInfo, nullptr, &renderer->views[i]);
    }
}

static void create_renderpass(Renderer *renderer)
{
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = VK_FORMAT_B8G8R8A8_SRGB;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    vkCreateRenderPass(renderer->device, &render_pass_info, nullptr, &renderer->pass);
}

static void create_framebuffers(Renderer *renderer)
{
    renderer->framebuffers = (VkFramebuffer *)malloc(sizeof(VkFramebuffer) * renderer->image_count);

    for (size_t i = 0; i < renderer->image_count; ++i) {
        VkImageView attachments[] = {
            renderer->views[i]
        };

        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = renderer->pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = WIDTH;
        framebuffer_info.height = HEIGHT;
        framebuffer_info.layers = 1;

        vkCreateFramebuffer(renderer->device, &framebuffer_info, nullptr, &renderer->framebuffers[i]);
    }
}

static void create_commandpool(Renderer *renderer, QueueFamilyIndices *indicies) 
{
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = indicies->graphics;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        vkCreateCommandPool(renderer->device, &pool_info, nullptr, &renderer->cmdpools[i].pool);

        renderer->cmdpools[i].buffers = (VkCommandBuffer *)malloc(sizeof(VkCommandBuffer) * renderer->image_count);

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = renderer->cmdpools[i].pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = renderer->image_count;

        vkAllocateCommandBuffers(renderer->device, &alloc_info, renderer->cmdpools[i].buffers);
    }
}

static void create_syncs(Renderer *renderer)
{
    renderer->available = (VkSemaphore *)malloc(sizeof(VkSemaphore) * FRAMES_IN_FLIGHT);
    renderer->rendered = (VkSemaphore *)malloc(sizeof(VkSemaphore) * FRAMES_IN_FLIGHT);
    renderer->inflight = (VkFence *)malloc(sizeof(VkFence) * FRAMES_IN_FLIGHT);
    renderer->image_inflight = (VkFence *)malloc(sizeof(VkFence) * renderer->image_count);

    for (size_t i = 0; i < renderer->image_count; ++i) {
        renderer->image_inflight[i] = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vkCreateSemaphore(renderer->device, &semaphore_info, nullptr, &renderer->available[i]);
        vkCreateSemaphore(renderer->device, &semaphore_info, nullptr, &renderer->rendered[i]);
        vkCreateFence(renderer->device, &fence_info, nullptr, &renderer->inflight[i]);
    }
}

void renderer_init(Renderer *renderer)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    renderer->window = glfwCreateWindow(WIDTH, HEIGHT, "Quack :>#", nullptr, nullptr);
    glfwSetInputMode(renderer->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    create_instance(renderer);
    glfwCreateWindowSurface(renderer->instance, renderer->window, nullptr, &renderer->surface);

    QueueFamilyIndices indicies;
    choose_physical(renderer, &indicies);

    fmt::print("graphics queue: {}\n", indicies.graphics);
    fmt::print("present queue: {}\n", indicies.present);

    create_logical(renderer, &indicies);
    create_swapchain(renderer, &indicies);
    create_imageviews(renderer);
    create_renderpass(renderer);
    create_framebuffers(renderer);
    create_commandpool(renderer, &indicies);
    create_syncs(renderer);

    fmt::print("{} image count.\n", renderer->image_count);
}

void begin_frame(Renderer *renderer, VkCommandBuffer *cmdbuffer)
{
    vkWaitForFences(renderer->device, 1, &renderer->inflight[renderer->current_frame], VK_TRUE, UINT64_MAX);

    vkAcquireNextImageKHR(renderer->device, renderer->swapchain, UINT64_MAX, renderer->available[renderer->current_frame], VK_NULL_HANDLE, &renderer->frame_index);

    if (renderer->image_inflight[renderer->frame_index] != VK_NULL_HANDLE) {
        vkWaitForFences(renderer->device, 1, &renderer->image_inflight[renderer->frame_index], VK_TRUE, UINT64_MAX);
    }
    renderer->image_inflight[renderer->frame_index] = renderer->inflight[renderer->current_frame];

    vkResetCommandPool(renderer->device, renderer->cmdpools[renderer->current_frame].pool, 0);
    *cmdbuffer = renderer->cmdpools[renderer->current_frame].buffers[renderer->frame_index];
}

void submit_frame(Renderer *renderer, VkCommandBuffer *cmdbuffer)
{
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { renderer->available[renderer->current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = cmdbuffer;

    VkSemaphore signal_semaphores[] = { renderer->rendered[renderer->current_frame] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(renderer->device, 1, &renderer->inflight[renderer->current_frame]);

    vkQueueSubmit(renderer->graphics_queue, 1, &submit_info, renderer->inflight[renderer->current_frame]);
    
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = { renderer->swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &renderer->frame_index;

    vkQueuePresentKHR(renderer->present_queue, &present_info);

    renderer->current_frame = (renderer->current_frame + 1) % FRAMES_IN_FLIGHT;
}

#endif