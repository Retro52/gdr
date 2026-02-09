#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <tracy/Tracy.hpp>

void render::destroy_buffer(VmaAllocator allocator, vk_buffer& buffer)
{
    ZoneScoped;
    buffer.size = 0;
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

result<render::vk_buffer> render::create_buffer(u64 size, VkBufferUsageFlags usage, VmaAllocator allocator,
                                                VmaAllocationCreateFlags allocation_flags)
{
    ZoneScoped;

    const VkBufferCreateInfo buffer_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = usage,
    };

    return render::create_buffer(buffer_info, allocator, allocation_flags);
}

result<render::vk_buffer> render::create_buffer(const VkBufferCreateInfo& buffer_create_info, VmaAllocator allocator,
                                                VmaAllocationCreateFlags allocation_flags)
{
    ZoneScoped;

    vk_buffer result {.size = buffer_create_info.size};
    const VmaAllocationCreateInfo alloc_info = {.flags = allocation_flags, .usage = VMA_MEMORY_USAGE_AUTO};
    VK_RETURN_ON_FAIL(
        vmaCreateBuffer(allocator, &buffer_create_info, &alloc_info, &result.buffer, &result.allocation, nullptr));

    return result;
}

result<render::vk_mapped_buffer> render::create_buffer_mapped(u64 size, VkBufferUsageFlags usage,
                                                              VmaAllocator allocator,
                                                              VmaAllocationCreateFlags allocation_flags)
{
    const VkBufferCreateInfo buffer_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = usage,
    };

    return render::create_buffer_mapped(buffer_info, allocator, allocation_flags);
}

result<render::vk_mapped_buffer> render::create_buffer_mapped(const VkBufferCreateInfo& buffer_create_info,
                                                              VmaAllocator allocator,
                                                              VmaAllocationCreateFlags allocation_flags)
{
    const auto& buffer = render::create_buffer(buffer_create_info, allocator, allocation_flags);
    if (!buffer)
    {
        return buffer.message;
    }

    vk_mapped_buffer result {.size = buffer->size, .buffer = buffer->buffer, .allocation = buffer->allocation};
    vmaMapMemory(allocator, buffer->allocation, &result.mapped);

    return result;
}

result<VkBufferView> render::create_buffer_view(VkDevice device, VkBuffer buffer, VkFormat format, u64 offset,
                                                u64 range)
{
    ZoneScoped;

    const VkBufferViewCreateInfo buffer_view_create_info = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .pNext  = nullptr,
        .buffer = buffer,
        .format = format,
        .offset = offset,
        .range  = range,
    };

    VkBufferView view;
    VK_RETURN_ON_FAIL(vkCreateBufferView(device, &buffer_view_create_info, nullptr, &view));

    return view;
}
