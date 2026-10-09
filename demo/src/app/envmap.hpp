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

        rhi::sampler sampler;
        rhi::sampler brdf_sampler;

        rhi::image_view conv_view;
        rhi::image convolution;

        rhi::image_view pref_view;
        rhi::image_view pref_mips[shader_constants::kEnvPrefilterMips];
        rhi::image prefiltered;

        rhi::image_view cube_view;
        rhi::image cubemap;

        rhi::image brdf_lut;

        envmap(const rhi::impl& impl, rhi::context ctx, rhi::image_format format, const envmap_config& cfg);

        void shutdown(const rhi::impl& impl, rhi::context ctx);

        [[nodiscard]] rhi::binding get_lut_descriptor_info() const;
        [[nodiscard]] rhi::binding get_cube_descriptor_info() const;
        [[nodiscard]] rhi::binding get_conv_descriptor_info() const;
        [[nodiscard]] rhi::binding get_pref_descriptor_info() const;

        void init(const rhi::impl& impl, rhi::command_buffer cmd, app::pso_data& pso);

        void load(const rhi::impl& impl, rhi::command_buffer cmd, app::pso_data& pso,
                  const app::gpu_upload_mgr& upload_mgr, const fs::path& path);
    };
}
