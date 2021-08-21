#ifndef RENDERER_BUFFERS_H
#define RENDERER_BUFFERS_H

#include <vulkan/vulkan.h>
#include <stdlib.h>

#include "core.h"

struct VertexBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

struct UniformBuffer {
    uint32_t count;
    VkBuffer *buffers;
    VkDeviceMemory *memories;
    void ** mapped;
};

static uint32_t find_memory_type(Renderer *renderer, uint32_t filter, VkMemoryPropertyFlags properties);
static void create_buffer(Renderer *renderer, VkBuffer *buffer, VkDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags type, VkMemoryPropertyFlags props);

static uint32_t find_memory_type(Renderer *renderer, uint32_t filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if ((filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
}

static void create_buffer(Renderer *renderer, VkBuffer *buffer, VkDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags type, VkMemoryPropertyFlags props)
{
    VkResult result;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = type;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(renderer->device, &buffer_info, nullptr, buffer);
    check_vulkan_result(result, "Failed to create buffer.");

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(renderer->device, *buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        renderer, 
        mem_requirements.memoryTypeBits, 
        props
    );

    result = vkAllocateMemory(renderer->device, &alloc_info, nullptr, memory);
    check_vulkan_result(result, "Failed to allocate buffer memory.");   

    result = vkBindBufferMemory(renderer->device, *buffer, *memory, 0);
    check_vulkan_result(result, "Failed to bind buffer memory.");
}

void create_vertex_buffer(Renderer *renderer, VertexBuffer *buffer, VkDeviceSize size)
{
    create_buffer(renderer, &buffer->buffer, &buffer->memory, size, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void fill_vertex_buffer(Renderer *renderer, VertexBuffer *buffer, void *data, size_t size)
{
    void *gpu_data;
    vkMapMemory(renderer->device, buffer->memory, 0, size, 0, &gpu_data);
    memcpy(gpu_data, data, size);
    vkUnmapMemory(renderer->device, buffer->memory);
}

void create_uniform_buffer(Renderer *renderer, UniformBuffer *uniform, VkDeviceSize size)
{
    uniform->count = renderer->image_count;

    uniform->buffers = (VkBuffer *)malloc(sizeof(VkBuffer) * uniform->count);
    uniform->memories = (VkDeviceMemory *)malloc(sizeof(VkDeviceMemory) * uniform->count);
    uniform->mapped = (void**)malloc(sizeof(void *) * uniform->count);

    for (size_t i = 0; i < uniform->count; ++i) {
        create_buffer(renderer, &uniform->buffers[i], &uniform->memories[i], size, 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkMapMemory(renderer->device, uniform->memories[i], 0, size, 0, &uniform->mapped[i]);
    }
}

void fill_uniform_mat4x4(Renderer *renderer, UniformBuffer *uniform, mat4x4 mat)
{
    memcpy(uniform->mapped[renderer->frame_index], mat, sizeof(float) * 16);
}

#endif