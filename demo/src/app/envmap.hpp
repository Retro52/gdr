#pragma once

#include <app/gpu_upload_mgr.hpp>
#include <fs/fs.hpp>
#include <render/rhi.hpp>
#include <shaders/constants.h>

namespace app
{
    struct pso_data;

    struct envmap_config
    {
        i32 env_resolution;
        i32 brdf_lut_resolution;
        i32 prefilter_resolution;
        i32 irradiance_resolution;
    };

    struct envmap
    {
        i32 env_resolution;
        i32 brdf_lut_resolution;
        i32 prefilter_resolution;
        i32 irradiance_resolution;

        render::rhi::sampler sampler;
        render::rhi::sampler brdf_sampler;

        render::rhi::image_view conv_view;
        render::rhi::image convolution;

        render::rhi::image_view pref_view;
        render::rhi::image_view pref_mips[shader_constants::kEnvPrefilterMips];
        render::rhi::image prefiltered;

        render::rhi::image_view cube_view;
        render::rhi::image cubemap;

        render::rhi::image brdf_lut;

        envmap(const render::rhi::rhi& rhi, render::rhi::context ctx, render::rhi::image_format format,
               const envmap_config& cfg);

        void shutdown(const render::rhi::rhi& rhi, render::rhi::context ctx);

        [[nodiscard]] render::rhi::binding get_lut_descriptor_info() const;
        [[nodiscard]] render::rhi::binding get_cube_descriptor_info() const;
        [[nodiscard]] render::rhi::binding get_conv_descriptor_info() const;
        [[nodiscard]] render::rhi::binding get_pref_descriptor_info() const;

        void init(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd, app::pso_data& pso);

        void load(const fs::path& path, app::pso_data& pso, const render::rhi::rhi& rhi, render::rhi::context ctx,
                  const app::gpu_upload_mgr& upload_mgr);
    };
}
