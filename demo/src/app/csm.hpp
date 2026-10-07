#pragma once

#include <app/render.hpp>
#include <cpp/containers/local_array.hpp>
#include <glm/mat4x4.hpp>
#include <render/rhi.hpp>
#include <shaders/types.h>

namespace app
{
    struct pso_data;

    struct csm_config
    {
        u32 resolution;

        f32 max_range;
        f32 split_lambda;
    };

    struct csm
    {
        u32 resolution;

        f32 max_range;
        f32 split_lambda;

        render::rhi::sampler sampler  = render::rhi::null_sampler;
        render::rhi::image shadow_map = render::rhi::null_image;
        cpp::local_array<render::rhi::image_view, 12> cascade_views;

        csm(const render::rhi::rhi& rhi, render::rhi::context ctx, render::rhi::image_format format,
            const csm_config& cfg);

        void init(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd);

        void shutdown(const render::rhi::rhi& rhi, render::rhi::context ctx);

        [[nodiscard]] render::rhi::binding get_descriptor_info() const;

        [[nodiscard]] f32 get_cascade_range(f32 near, u32 index) const;

        [[nodiscard]] glm::mat4 get_light_view_matrix(const vec3& light_dir) const;

        [[nodiscard]] glm::mat4 get_cascade_inv_vp(f32 camera_near, f32 camera_ratio, f32 camera_fov,
                                                   const glm::mat4& camera_view, u32 cascade) const;

        [[nodiscard]] vec4 get_cascade_sphere(const glm::mat4& vp_inverse) const;

        [[nodiscard]] shader_types::Bounds3D get_cascade_bounds(const vec4& sphere, const glm::mat4& light_view) const;
    };
}
