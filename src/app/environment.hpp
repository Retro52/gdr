#pragma once

#include <app/render.hpp>
#include <fs/fs.hpp>

namespace render
{
    class vk_renderer;
    struct vk_buffer_transfer;
}

namespace app
{
    struct pso_data;

    struct environment
    {
        i32 resolution;
        i32 prefilter_resolution;
        cpp::tagged_int<i32, 1> irradiance_resolution;

        VkSampler sampler;
        VkImageView cube_view;
        render::vk_image cubemap;

        VkImageView conv_view;
        render::vk_image convolution;

        VkImageView pref_view;
        render::vk_image prefiltered;

        environment(const render::vk_renderer& renderer, VkFormat format, i32 resolution, i32 irradiance_resolution,
                    i32 prefilter_resolution);

        void shutdown(const render::vk_renderer& renderer);

        [[nodiscard]] bool valid() const;
        render::vk_descriptor_info get_cube_descriptor_info() const;
        render::vk_descriptor_info get_conv_descriptor_info() const;

        void load(const fs::path& path, app::pso_data& pso, render::vk_renderer& renderer,
                  const render::vk_buffer_transfer& transfer);
    };
}
