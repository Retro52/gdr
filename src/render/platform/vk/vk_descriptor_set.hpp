#pragma once

#include <volk.h>
#include <result.hpp>

namespace render
{
    struct vk_descriptor_set
    {
        VkDescriptorSet descriptor_set;
        VkDescriptorPool descriptor_pool;
        VkDescriptorSetLayout descriptor_set_layout;
    };

    void destroy_descriptor_set(VkDevice device, vk_descriptor_set& descriptor_pool);

    result<vk_descriptor_set> create_bindless_textures_set(VkDevice device, u32 max_textures);
}
