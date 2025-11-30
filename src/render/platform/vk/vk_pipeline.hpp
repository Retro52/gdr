#pragma once

#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <result.hpp>

namespace render
{
    class pipeline
    {
    public:
        struct push_constants
        {
            u32 size;
            u32 offset;
        };

    public:
        static result<pipeline> create_graphics(const vk_renderer& renderer, VkShaderModule vertex,
                                                VkShaderModule fragment, const VkPushConstantRange* push_constants,
                                                u32 pc_count, VkVertexInputBindingDescription vertex_bind,
                                                const VkVertexInputAttributeDescription* vertex_attr,
                                                u32 vertex_attr_count);

        void bind(VkCommandBuffer command_buffer) const;

        void push_constant(VkCommandBuffer command_buffer, u32 size, const void* data) const;

        template<typename T>
        void push_constant(VkCommandBuffer command_buffer, T&& data) const
        {
            push_constant(command_buffer, sizeof(T), &data);
        }

    private:
        explicit pipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                          VkPipelineBindPoint pipeline_bind_point);

    private:
        VkDevice m_device;
        VkPipeline m_pipeline {VK_NULL_HANDLE};
        VkPipelineLayout m_pipeline_layout {VK_NULL_HANDLE};
        VkPipelineBindPoint m_pipeline_bind_point {VK_PIPELINE_BIND_POINT_GRAPHICS};
    };
}
