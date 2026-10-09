#define GLM_ENABLE_EXPERIMENTAL
#include <app/csm.hpp>
#include <app/pso.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <scene/matrix_common.hpp>
#include <shaders/types.h>

#include <array>

static std::array<vec4, 8> get_frustum_corners_world(const glm::mat4& pv_inverse)
{
    std::array<vec4, 8> corners {};
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const vec4 pt = pv_inverse
                              * vec4(2.0f * static_cast<f32>(x) - 1.0f,
                                     2.0f * static_cast<f32>(y) - 1.0f,
                                     static_cast<f32>(z),
                                     1.0f);
                corners[x * 4 + y * 2 + z] = pt / pt.w;
            }
        }
    }

    return corners;
}

static vec3 get_corners_center(const std::array<glm::vec4, 8>& corners)
{
    glm::vec3 center {0, 0, 0};
    for (const auto& v : corners)
    {
        center += glm::vec3(v);
    }

    return center / static_cast<f32>(corners.size());
}

app::csm::csm(const rhi::impl& rhi, const rhi::context ctx, const rhi::image_format format, const csm_config& cfg)
    : resolution(cfg.resolution)
    , max_range(cfg.max_range)
    , split_lambda(cfg.split_lambda)
{
    sampler = *rhi.create_sampler(ctx,
                                  {
                                      .address_mode = rhi::sampler_address_mode::clamp_to_border,
                                      .compare_op   = rhi::compare_op::equal_or_greater,
                                      .border_color = rhi::sampler_border_color::black,
                                  });

    const rhi::create_image_info image_create_info {
        .format = format,

        .mips_count  = 1,
        .layer_count = shader_constants::kMaxShadowCascades,

        .dimensions  = uvec3(resolution, resolution, 1),
        .usage_flags = rhi::image_usage::sampled | rhi::image_usage::transfer_dst | rhi::image_usage::attachment_ds,
    };

    shadow_map = *rhi.create_image(ctx, image_create_info);

    cascade_views.resize(shader_constants::kMaxShadowCascades);
    for (u32 i = 0; i < shader_constants::kMaxShadowCascades; ++i)
    {
        rhi::create_image_view_info image_view_info {
            .range  = {.layers_range = {i, 1}},
            .format = format,
        };
        cascade_views[i] = *rhi.create_image_view(ctx, shadow_map, image_view_info);
    }
}

void app::csm::init(const rhi::impl& rhi, const rhi::command_buffer cmd)
{
    const rhi::image_barrier barrier =
        app::make_image_barrier(shadow_map, rhi::image_layout::common, rhi::image_aspect::depth);

    const rhi::barrier_batch barriers {
        .images = {&barrier, 1}
    };

    rhi.cmd_barriers(cmd, barriers);
}

void app::csm::shutdown(const rhi::impl& rhi, const rhi::context ctx)
{
    for (auto& view : cascade_views)
    {
        rhi.destroy_image_view(ctx, view);
    }

    cascade_views.clear();

    rhi.destroy_sampler(ctx, sampler);
    rhi.destroy_image(ctx, shadow_map);
}

rhi::binding app::csm::get_descriptor_info() const
{
    return {shadow_map, sampler};
}

[[nodiscard]] f32 app::csm::get_cascade_range(const f32 near, const u32 index) const
{
    const f32 p           = static_cast<f32>(index + 1) / static_cast<f32>(shader_constants::kMaxShadowCascades);
    const f32 logarithmic = near * std::pow(max_range / near, p);
    const f32 uniform     = near + (max_range - near) * p;

    return glm::mix(uniform, logarithmic, split_lambda);
}

[[nodiscard]] glm::mat4 app::csm::get_light_view_matrix(const vec3& light_dir) const
{
    const auto up = glm::abs(light_dir.y) > 0.99F ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    return glm::lookAt(light_dir, glm::vec3(0.0F), up);
}

[[nodiscard]] glm::mat4 app::csm::get_cascade_inv_vp(const f32 camera_near, const f32 camera_ratio,
                                                     const f32 camera_fov, const glm::mat4& camera_view,
                                                     const u32 cascade) const
{
    const f32 cascade_near = cascade == 0 ? camera_near : get_cascade_range(camera_near, cascade - 1);
    const f32 cascade_far  = get_cascade_range(camera_near, cascade);
    const auto proj        = scene::get_projection_matrix(cascade_near, cascade_far, camera_ratio, camera_fov);
    return glm::inverse(proj * camera_view);
}

[[nodiscard]] vec4 app::csm::get_cascade_sphere(const glm::mat4& vp_inverse) const
{
    const auto corners = get_frustum_corners_world(vp_inverse);
    const auto center  = get_corners_center(corners);

    f32 radius = 0.0F;
    for (const auto& corner : corners)
    {
        radius = std::max(radius, glm::length(glm::vec3(corner) - center));
    }

    return {center, radius};
}

[[nodiscard]] shader_types::Bounds3D app::csm::get_cascade_bounds(const vec4& sphere, const glm::mat4& light_view) const
{
    const f32 texel = 2.0F * sphere.w / static_cast<f32>(resolution);

    auto center = glm::vec3(light_view * glm::vec4(vec3(sphere), 1.0F));
    center.x    = std::floor(center.x / texel) * texel;
    center.y    = std::floor(center.y / texel) * texel;

    shader_types::Bounds3D bounds;
    bounds.min = center - glm::vec3(sphere.w);
    bounds.max = center + glm::vec3(sphere.w);

    return bounds;
}
