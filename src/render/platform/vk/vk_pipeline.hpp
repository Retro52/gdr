#pragma once

#include <bytes.hpp>
#include <fs/path.hpp>
#include <nlohmann/json.hpp>
#include <render/platform/vk/vk_descriptor_set.hpp>
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

    struct vk_descriptor_bindings
    {
        constexpr static u32 kMaxSetZeroBindings = 32;
        render::vk_descriptor_info render_bindings[kMaxSetZeroBindings] {};

        render::vk_descriptor_info* get() { return render_bindings; }

        auto& bind_at(const render::vk_descriptor_info& next, u32 index)
        {
            assert2(index < kMaxSetZeroBindings);
            render_bindings[index] = next;
            return *this;
        }
    };

    struct vk_shader
    {
        struct shader_meta
        {
            VkDescriptorType bindings[32];
            u32 work_group_size[3];
            u32 bindings_count            = 0;
            u32 max_binding_set_used      = 0;
            u32 push_constant_struct_size = 0;
            VkShaderStageFlagBits stage   = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

            shader_meta()
                : work_group_size {}
            {
                cpp::cx_fill(bindings + 0, bindings + COUNT_OF(bindings), VK_DESCRIPTOR_TYPE_MAX_ENUM);
            }
        };

        VkShaderModule module {VK_NULL_HANDLE};
        shader_meta meta;

    public:
        static shader_meta parse_spirv(const bytes& spv);

        static result<vk_shader> load(const vk_renderer& renderer, const fs::path& path);
    };

    struct vk_pipeline
    {
        VkPipeline m_pipeline {VK_NULL_HANDLE};
        VkPipelineLayout m_pipeline_layout {VK_NULL_HANDLE};
        VkDescriptorSetLayout m_desc_set_layout {VK_NULL_HANDLE};
        VkDescriptorUpdateTemplate m_descriptor_update_template {VK_NULL_HANDLE};

        VkPipelineBindPoint m_pipeline_bind_point {VK_PIPELINE_BIND_POINT_GRAPHICS};
        VkShaderStageFlags m_push_constant_stages {VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM};
        u32 m_push_constants_max_size;
        u32 work_group_size[3] {};

        static result<vk_pipeline> create_compute(const vk_renderer& renderer, const vk_shader& shader,
                                                  const vk_descriptor_set* desc_set = nullptr, u32 desc_set_count = 0);

        static result<vk_pipeline> create_graphics(const vk_renderer& renderer, const vk_shader* shaders,
                                                   u32 shaders_count, const vk_descriptor_set* desc_set = nullptr,
                                                   u32 desc_set_count            = 0,
                                                   const nlohmann::json& options = nlohmann::json());

        void bind(VkCommandBuffer command_buffer) const;

        void push_constant(VkCommandBuffer command_buffer, u32 size, const void* data) const;

        void bind_descriptor_set(VkCommandBuffer command_buffer, const vk_descriptor_set& set) const;

        void push_descriptor_set(VkCommandBuffer command_buffer, const vk_descriptor_info* updates) const;

        void dispatch(VkCommandBuffer command_buffer, u32 global_x, u32 global_y, u32 global_z) const;

        template<typename T>
        void push_constant(VkCommandBuffer command_buffer, T&& data) const
        {
            push_constant(command_buffer, sizeof(T), &data);
        }
    };

    void destroy_shader(VkDevice device, vk_shader& shader);

    void destroy_pipeline(VkDevice device, vk_pipeline& pso);
}
