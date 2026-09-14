#define GLM_ENABLE_EXPERIMENTAL
#include <app/csm.hpp>
#include <app/pso.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <render/platform/vk/vk_barrier.hpp>
#include <render/platform/vk/vk_buffer_transfer.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <scene/matrix_common.hpp>
#include <shaders/types.h>

static std::array<glm::vec4, 8> get_frustum_corners_world(const glm::mat4& pv_inverse)
{
    std::array<glm::vec4, 8> corners {};
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const glm::vec4 pt = pv_inverse
                                   * glm::vec4(2.0f * static_cast<f32>(x) - 1.0f,
                                               2.0f * static_cast<f32>(y) - 1.0f,
                                               static_cast<f32>(z),
                                               1.0f);
                corners[x * 4 + y * 2 + z] = pt / pt.w;
            }
        }
    }

    return corners;
}

static glm::vec3 get_corners_center(const std::array<glm::vec4, 8>& corners)
{
    glm::vec3 center {0, 0, 0};
    for (const auto& v : corners)
    {
        center += glm::vec3(v);
    }

    return center / static_cast<f32>(corners.size());
}

app::csm::csm(const render::vk_renderer& renderer, VkFormat format, const csm_config& cfg)
    : resolution(cfg.resolution)
    , max_range(cfg.max_range)
    , split_lambda(cfg.split_lambda)
{
    const VkImageCreateInfo image_create_info {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = format,
        .extent      = {resolution, resolution, 1},
        .mipLevels   = 1,
        .arrayLayers = shader_constants::kMaxShadowCascades,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage =
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    sampler = *render::create_sampler(renderer.get_context().device,
                                      VK_FILTER_LINEAR,
                                      VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      VK_SAMPLER_REDUCTION_MODE_MAX_ENUM,
                                      0.0F,
                                      VK_COMPARE_OP_GREATER_OR_EQUAL,
                                      VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK);

    shadow_map = *render::create_image(
        renderer.get_context().device, image_create_info, VK_IMAGE_ASPECT_DEPTH_BIT, renderer.get_context().allocator);

    cascade_views.resize(shader_constants::kMaxShadowCascades);
    for (u32 i = 0; i < shader_constants::kMaxShadowCascades; ++i)
    {
        cascade_views[i] = *render::create_image_array_view(renderer.get_context().device,
                                                            shadow_map.image,
                                                            VK_IMAGE_VIEW_TYPE_2D,
                                                            format,
                                                            VK_IMAGE_ASPECT_DEPTH_BIT,
                                                            i,
                                                            1);
    }
}

void app::csm::init(const render::vk_renderer& renderer)
{
    renderer.submit(
        [&](VkCommandBuffer cmd)
        {
            render::transition_image(
                cmd, shadow_map.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        });
}

void app::csm::shutdown(const render::vk_renderer& renderer)
{
    for (const auto& view : cascade_views)
    {
        vkDestroyImageView(renderer.get_context().device, view, nullptr);
    }

    vkDestroySampler(renderer.get_context().device, sampler, nullptr);
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, shadow_map);
}

render::vk_descriptor_info app::csm::get_descriptor_info() const
{
    return {sampler, shadow_map.view, VK_IMAGE_LAYOUT_GENERAL};
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
