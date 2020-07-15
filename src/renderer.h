#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fmt/core.h>
#include <stdio.h>

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
    VkSemaphore *available;
    VkSemaphore *rendered;
    VkFence *inflight;
    VkFence *image_inflight;
};

typedef enum RenderResult {
    RENDER_SUCCESS = 0,
    RENDER_NOFILE = -5
} RenderResult;

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

static RenderResult read_shader(const char *filename, char **source, size_t *size)
{
    FILE *f = fopen(filename, "rb");

    if (f == nullptr) {
        return RENDER_NOFILE;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    *source = (char *)malloc(*size);
    fread(*source, 1, *size, f);
    fclose(f);

    return RENDER_SUCCESS;
}

static void create_shader(Renderer *renderer, char *source, size_t size, VkShaderModule *shader) 
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = (uint32_t *)source;

    vkCreateShaderModule(renderer->device, &create_info, nullptr, shader);
}

void renderer_pipeline(Renderer *renderer, VkPipeline *pipeline)
{
    char *frag_source,
         *vert_source;

    size_t vert_size,
           frag_size;

    read_shader("../data/shaders/tri_vert.spv", &vert_source, &vert_size);
    read_shader("../data/shaders/tri_frag.spv", &frag_source, &frag_size);

    fmt::print("vert source size: {}\n", vert_size);
    fmt::print("frag source size: {}\n", frag_size);

    VkShaderModule vert_shader;
    VkShaderModule frag_shader;

    create_shader(renderer, vert_source, vert_size, &vert_shader);
    create_shader(renderer, frag_source, frag_size, &frag_shader);

    // free(vert_source);
    // free(frag_source);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vert_shader;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = frag_shader;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 1.0f * WIDTH;
    viewport.height = 1.0f * HEIGHT;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = VkExtent2D{WIDTH, HEIGHT};

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(renderer->device, &pipelineLayoutInfo, nullptr, &layout);

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderer->pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline);
}

void renderer_init(Renderer *renderer)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    renderer->window = glfwCreateWindow(WIDTH, HEIGHT, "Quack :>#", nullptr, nullptr);

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

static void record_commandbuffer(Renderer *renderer, VkPipeline pipeline, VkCommandBuffer buffer, uint32_t index)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(buffer, &beginInfo);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderer->pass;
    renderPassInfo.framebuffer = renderer->framebuffers[index];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = VkExtent2D{WIDTH, HEIGHT};

    VkClearValue clearColor = {.003f, .003f, .003f, 1.0f};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdDraw(buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(buffer);

    vkEndCommandBuffer(buffer);
}

void drawframe(Renderer *renderer, VkPipeline pipeline)
{
    uint32_t current_frame = renderer->current_frame;

    vkWaitForFences(renderer->device, 1, &renderer->inflight[current_frame], VK_TRUE, UINT64_MAX);

    uint32_t index;
    vkAcquireNextImageKHR(renderer->device, renderer->swapchain, UINT64_MAX, renderer->available[current_frame], VK_NULL_HANDLE, &index);

/*
    if (renderer->image_inflight[index] != VK_NULL_HANDLE) {
        vkWaitForFences(renderer->device, 1, &renderer->image_inflight[index], VK_TRUE, UINT64_MAX);
    }
    renderer->image_inflight[index] = renderer->inflight[current_frame];
*/

    vkResetCommandPool(renderer->device, renderer->cmdpools[current_frame].pool, 0);
    record_commandbuffer(renderer, pipeline, renderer->cmdpools[current_frame].buffers[index], index);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { renderer->available[current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &renderer->cmdpools[current_frame].buffers[index];

    VkSemaphore signal_semaphores[] = { renderer->rendered[current_frame] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(renderer->device, 1, &renderer->inflight[current_frame]);

    vkQueueSubmit(renderer->graphics_queue, 1, &submit_info, renderer->inflight[current_frame]);
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapChains[] = { renderer->swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &index;

    vkQueuePresentKHR(renderer->present_queue, &presentInfo);

    renderer->current_frame = (renderer->current_frame + 1) % FRAMES_IN_FLIGHT;
}