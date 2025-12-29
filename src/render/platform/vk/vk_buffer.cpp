#include <render/platform/vk/vk_buffer.hpp>
#include <Tracy/Tracy.hpp>

void render::destroy_buffer(VmaAllocator allocator, const vk_buffer& buffer)
{
    ZoneScoped;
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

VkResult render::create_buffer(u64 size, VkBufferUsageFlags usage, VmaAllocator allocator,
                               VmaAllocationCreateFlags allocation_flags, vk_buffer* buffer)
{
    ZoneScoped;

    const VkBufferCreateInfo buffer_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = usage,
    };

    return render::create_buffer(buffer_info, allocator, allocation_flags, buffer);
}

VkResult render::create_buffer(const VkBufferCreateInfo& buffer_create_info, VmaAllocator allocator,
                               VmaAllocationCreateFlags allocation_flags, vk_buffer* buffer)
{
    ZoneScoped;

    const VmaAllocationCreateInfo alloc_info = {.flags = allocation_flags, .usage = VMA_MEMORY_USAGE_AUTO};
    return vmaCreateBuffer(allocator, &buffer_create_info, &alloc_info, &buffer->buffer, &buffer->allocation, nullptr);
}

VkResult render::create_buffer_view(VkDevice device, VkBuffer buffer, VkFormat format, u64 offset, u64 range,
                                    VkBufferView* view)
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

    return vkCreateBufferView(device, &buffer_view_create_info, nullptr, view);
}
