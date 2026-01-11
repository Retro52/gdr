#pragma once

#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>

namespace render::debug
{
    class frustum_renderer
    {
    private:
        struct frustum_pc_data
        {
            glm::mat4 renderer_vp;
            glm::mat4 target_inv_vp;
        };

    public:
        explicit frustum_renderer(const vk_renderer& renderer)
        {
            render::vk_shader shaders[] = {
                *render::vk_shader::load(renderer, "../shaders/frustum.vert.spv"),
                *render::vk_shader::load(renderer, "../shaders/frustum.frag.spv"),
            };

            // TODO: FINALLY parse this stuff from shader & fix the VK_SHADER_STAGE_ALL requirement
            const VkPushConstantRange range {
                .stageFlags = VK_SHADER_STAGE_ALL,
                .offset     = 0,
                .size       = sizeof(frustum_pc_data),
            };

            m_pipeline = *render::vk_pipeline::create_graphics(
                renderer, shaders, COUNT_OF(shaders), &range, 1, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        }

        void draw(VkCommandBuffer cmd, const glm::mat4& camera_vp, const glm::mat4& target_view,
                  const glm::mat4& target_proj)
        {
            const frustum_pc_data pc {
                .renderer_vp   = camera_vp,
                .target_inv_vp = glm::inverse(target_proj * target_view),
            };

            m_pipeline.bind(cmd);
            m_pipeline.push_constant(cmd, pc);
            vkCmdDraw(cmd, 24, 1, 0, 0);
        }

    private:
        vk_pipeline m_pipeline;
    };
}
