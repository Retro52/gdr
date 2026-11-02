#pragma once

#include <pod_types.hpp>
#include <result.hpp>

#include <volk.h>

namespace platform
{
    struct vk_descriptor_set
    {
        VkDescriptorSet descriptor_set;
        VkDescriptorPool descriptor_pool;
        VkDescriptorSetLayout descriptor_set_layout;
    };

    void vk_destroy_descriptor_set(VkDevice device, vk_descriptor_set& descriptor_pool);

    result<vk_descriptor_set> vk_create_bindless_textures_set(VkDevice device, u32 max_textures);
}
