#pragma once

#include <fs/path.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <result.hpp>

namespace render
{
    struct vk_descriptor_info
    {
        union
        {
            VkDescriptorImageInfo m_image;
            VkDescriptorBufferInfo m_buffer;
        };

        vk_descriptor_info() = default;

        vk_descriptor_info(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
        {
            m_image.sampler     = sampler;
            m_image.imageView   = imageView;
            m_image.imageLayout = imageLayout;
        }

        vk_descriptor_info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
        {
            m_buffer.buffer = buffer;
            m_buffer.offset = offset;
            m_buffer.range  = range;
        }

        vk_descriptor_info(VkBuffer buffer)
        {
            m_buffer.buffer = buffer;
            m_buffer.offset = 0;
            m_buffer.range  = VK_WHOLE_SIZE;
        }
    };

    class vk_shader
    {
    public:
        struct shader_meta
        {
            VkShaderStageFlagBits stage;
            VkDescriptorSetLayout set_layout;
        };

    public:
        static result<vk_shader> load(const vk_renderer& renderer, const fs::path& path);

    private:
        static shader_meta parse_spirv(const bytes& spv);

    public:
        VkShaderModule module;
        shader_meta meta;
    };

    class vk_pipeline
    {
    public:
        struct push_constants
        {
            u32 size;
            u32 offset;
        };

    public:
        static result<vk_pipeline> create_graphics(const vk_renderer& renderer, const vk_shader* shaders,
                                                   u32 shaders_count, const VkPushConstantRange* push_constants,
                                                   u32 pc_count);

        void bind(VkCommandBuffer command_buffer) const;

        void push_constant(VkCommandBuffer command_buffer, u32 size, const void* data) const;

        void push_descriptor_set(VkCommandBuffer command_buffer, const vk_descriptor_info* updates) const;

        template<typename T>
        void push_constant(VkCommandBuffer command_buffer, T&& data) const
        {
            push_constant(command_buffer, sizeof(T), &data);
        }

    private:
        explicit vk_pipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                             VkDescriptorUpdateTemplate update_template, VkPipelineBindPoint pipeline_bind_point);

    private:
        VkDevice m_device;
        VkPipeline m_pipeline {VK_NULL_HANDLE};
        VkPipelineLayout m_pipeline_layout {VK_NULL_HANDLE};
        VkDescriptorUpdateTemplate m_descriptor_update_template {VK_NULL_HANDLE};
        VkPipelineBindPoint m_pipeline_bind_point {VK_PIPELINE_BIND_POINT_GRAPHICS};
    };
}
