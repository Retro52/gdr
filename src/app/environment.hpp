#pragma once

#include <app/render.hpp>
#include <fs/fs.hpp>
#include <shaders/constants.h>

namespace render
{
    class vk_renderer;
    struct vk_buffer_transfer;
}

namespace app
{
    struct pso_data;

    struct environment_config
    {
        i32 env_resolution;
        i32 brdf_lut_resolution;
        i32 prefilter_resolution;
        i32 irradiance_resolution;
    };

    struct environment
    {
        i32 env_resolution;
        i32 brdf_lut_resolution;
        i32 prefilter_resolution;
        i32 irradiance_resolution;

        VkSampler sampler;
        VkSampler brdf_sampler;

        VkImageView conv_view;
        render::vk_image convolution;

        VkImageView pref_view;
        VkImageView pref_mips[shader_constants::kEnvPrefilterMips];
        render::vk_image prefiltered;

        VkImageView cube_view;
        render::vk_image cubemap;

        render::vk_image brdf_lut;

        environment(const render::vk_renderer& renderer, VkFormat format, const environment_config& cfg);

        void shutdown(const render::vk_renderer& renderer);

        [[nodiscard]] render::vk_descriptor_info get_lut_descriptor_info() const;
        [[nodiscard]] render::vk_descriptor_info get_cube_descriptor_info() const;
        [[nodiscard]] render::vk_descriptor_info get_conv_descriptor_info() const;
        [[nodiscard]] render::vk_descriptor_info get_pref_descriptor_info() const;

        void init(app::pso_data& pso, render::vk_renderer& renderer);

        void load(const fs::path& path, app::pso_data& pso, render::vk_renderer& renderer,
                  const render::vk_buffer_transfer& transfer);
    };
}
