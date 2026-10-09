#include <app/envmap.hpp>
#include <app/pso.hpp>
#include <app/render.hpp>
#include <log.hpp>
#include <scene/loader.hpp>

static u32 get_mips_count(const i32 resolution)
{
    return 1u + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(resolution))));
}

static void generate_mips(const rhi::impl& impl, const rhi::command_buffer cmd, const rhi::image cube, i32 resolution)
{
    const u32 mips = get_mips_count(resolution);
    for (u32 i = 1; i < mips; ++i)
    {
        const i32 mip_resolution = resolution > 1 ? resolution / 2 : resolution;
        impl.cmd_image_blit(cmd,
                            cube,
                            cube,
                            rhi::blit_image_info {
                                .src_extent = ivec3(resolution, resolution, 1),
                                .src_mip    = i - 1,

                                .dst_extent = ivec3(mip_resolution, mip_resolution, 1),
                                .dst_mip    = i,
                            });

        rhi::image_barrier barriers[] = {
            app::make_image_barrier(cube,
                                    rhi::image_layout::current,
                                    rhi::image_layout::current,
                                    rhi::barrier_stage::copy,
                                    rhi::barrier_stage::copy,
                                    rhi::barrier_access::copy_write,
                                    rhi::barrier_access::copy_write | rhi::barrier_access::copy_read,
                                    rhi::image_aspect::color),
        };

        impl.cmd_barriers(cmd, rhi::barrier_batch {.images = std::span {barriers}});
        resolution = mip_resolution;
    }
}

static void make_brdf_lut(const rhi::impl& impl, const rhi::command_buffer cmd, const rhi::pipeline pso,
                          const rhi::image brdf_lut, i32 resolution)
{
    const rhi::binding bindings[] = {
        {brdf_lut},
    };

    impl.cmd_bind_pso(cmd, pso);
    impl.cmd_push_constants(cmd, pso, &resolution, sizeof(resolution), 0);
    impl.cmd_push_bindings(cmd, pso, std::span {bindings});
    impl.cmd_dispatch(cmd, pso, {resolution, resolution, 1});

    rhi::global_barrier barrier {
        .before = {.stages = rhi::barrier_stage::compute_shader,                                       .access = rhi::barrier_access::storage_write},
        .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::fragment_shader,
                   .access = rhi::barrier_access::sampled_read                                                                                     },
    };

    impl.cmd_barriers(cmd,
                      {
                          .globals = std::span {&barrier, 1}
    });
}

app::envmap::envmap(const rhi::impl& impl, rhi::context ctx, rhi::image_format format, const envmap_config& cfg)
    : env_resolution(cfg.env_resolution)
    , brdf_lut_resolution(cfg.brdf_lut_resolution)
    , prefilter_resolution(cfg.prefilter_resolution)
    , irradiance_resolution(cfg.irradiance_resolution)

{
    rhi::create_image_info image_create_info {.format      = format,
                                              .layer_count = 6,
                                              .usage_flags = rhi::image_usage::transfer_dst
                                                           | rhi::image_usage::transfer_src | rhi::image_usage::sampled
                                                           | rhi::image_usage::storage};

    {
        image_create_info.mips_count = get_mips_count(env_resolution);
        image_create_info.dimensions = {env_resolution, env_resolution, 1},
        cubemap                      = *impl.create_image(ctx, image_create_info);
    }

    {
        image_create_info.mips_count = 1;
        image_create_info.dimensions = {irradiance_resolution, irradiance_resolution, 1},
        convolution                  = *impl.create_image(ctx, image_create_info);
    }

    {
        image_create_info.mips_count = shader_constants::kEnvPrefilterMips;
        image_create_info.dimensions = {prefilter_resolution, prefilter_resolution, 1},
        prefiltered                  = *impl.create_image(ctx, image_create_info);

        for (u32 i = 0; i < shader_constants::kEnvPrefilterMips; ++i)
        {
            rhi::create_image_view_info image_view_info {
                .range  = {.mips_range = {i, 1}},
                .format = format,
                .kind   = rhi::image_view_kind::array_2d,
            };

            pref_mips[i] = *impl.create_image_view(ctx, prefiltered, image_view_info);
        }
    }

    {
        image_create_info.mips_count  = 1;
        image_create_info.layer_count = 1;
        image_create_info.format      = rhi::image_format::r16g16sf;
        image_create_info.usage_flags = rhi::image_usage::sampled | rhi::image_usage::storage,

        image_create_info.dimensions = {brdf_lut_resolution, brdf_lut_resolution, 1},
        brdf_lut                     = *impl.create_image(ctx, image_create_info);
    }

    sampler = *impl.create_sampler(ctx, {.anisotropy_factor = 16});
    brdf_sampler =
        *impl.create_sampler(ctx, {.address_mode = rhi::sampler_address_mode::clamp_to_edge, .anisotropy_factor = 16});

    cube_view = *impl.create_image_view(ctx, cubemap, {.format = format, .kind = rhi::image_view_kind::flat_cube});
    conv_view = *impl.create_image_view(ctx, convolution, {.format = format, .kind = rhi::image_view_kind::flat_cube});
    pref_view = *impl.create_image_view(ctx, prefiltered, {.format = format, .kind = rhi::image_view_kind::flat_cube});
}

void app::envmap::shutdown(const rhi::impl& impl, rhi::context ctx)
{
    impl.destroy_sampler(ctx, sampler);
    impl.destroy_sampler(ctx, brdf_sampler);

    impl.destroy_image_view(ctx, cube_view);
    impl.destroy_image_view(ctx, conv_view);
    impl.destroy_image_view(ctx, pref_view);

    for (auto& view : pref_mips)
    {
        impl.destroy_image_view(ctx, view);
    }

    impl.destroy_image(ctx, cubemap);
    impl.destroy_image(ctx, brdf_lut);
    impl.destroy_image(ctx, convolution);
    impl.destroy_image(ctx, prefiltered);
}

rhi::binding app::envmap::get_lut_descriptor_info() const
{
    return {brdf_lut, brdf_sampler};
}

rhi::binding app::envmap::get_cube_descriptor_info() const
{
    return {cube_view, sampler};
}

rhi::binding app::envmap::get_conv_descriptor_info() const
{
    return {conv_view, sampler};
}

rhi::binding app::envmap::get_pref_descriptor_info() const
{
    return {pref_view, sampler};
}

void app::envmap::init(const rhi::impl& impl, rhi::command_buffer cmd, app::pso_data& pso)
{
    rhi::image_barrier barriers[] {
        app::make_image_barrier(cubemap, rhi::image_layout::common),
        app::make_image_barrier(brdf_lut, rhi::image_layout::common),
        app::make_image_barrier(convolution, rhi::image_layout::common),
        app::make_image_barrier(prefiltered, rhi::image_layout::common),
    };

    impl.cmd_barriers(cmd, {.images = std::span {barriers}});
    make_brdf_lut(impl, cmd, pso[pso_id::make_brdf_lookup_pipeline], brdf_lut, brdf_lut_resolution);
}

void app::envmap::load(const rhi::impl& impl, rhi::command_buffer cmd, app::pso_data& pso,
                       const app::gpu_upload_mgr& upload_mgr, const fs::path& path)
{
    if (auto equirect = loader::load_texture(path))
    {
        const auto equirect_img = upload_mgr.create_texture(*equirect);

        {
            // TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "unpack equirect"));

            const rhi::binding pass_bindings[] = {
                rhi::binding(equirect_img, sampler),
                rhi::binding(cubemap),
            };

            const rhi::pipeline& pass = pso[pso_id::equirect_unpack_pipeline];

            impl.cmd_bind_pso(cmd, pass);
            impl.cmd_push_constants(cmd, pass, &env_resolution, sizeof(env_resolution), 0);
            impl.cmd_push_bindings(cmd, pass, std::span {pass_bindings});
            impl.cmd_dispatch(cmd, pass, uvec3(env_resolution, env_resolution, 6));

            rhi::global_barrier barriers[] {
                rhi::global_barrier {
                                     .before = {.stages = rhi::barrier_stage::compute_shader,
                               .access = rhi::barrier_access::storage_write},
                                     .after  = {.stages = rhi::barrier_stage::all_commands,
                               .access = rhi::barrier_access::copy_read | rhi::barrier_access::copy_write},
                                     },
            };

            impl.cmd_barriers(cmd, {.globals = std::span {barriers}});
        }

        {
            // TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap mips generation"));
            generate_mips(impl, cmd, cubemap, env_resolution);

            rhi::global_barrier barriers[] {
                rhi::global_barrier {
                                     .before = {.stages = rhi::barrier_stage::copy, .access = rhi::barrier_access::copy_write},
                                     .after  = {.stages = rhi::barrier_stage::compute_shader,
                               .access = rhi::barrier_access::sampled_read},
                                     },
            };

            impl.cmd_barriers(cmd, {.globals = std::span {barriers}});
        }

        {
            // TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap convolution"));

            const rhi::binding pass_bindings[] = {
                rhi::binding(cube_view, sampler),
                rhi::binding(convolution),
            };

            const rhi::pipeline& pass = pso[pso_id::cubemap_convolute_pipeline];

            int push_constants[] = {irradiance_resolution, env_resolution};
            impl.cmd_bind_pso(cmd, pass);
            impl.cmd_push_constants(cmd, pass, push_constants, sizeof(push_constants), 0);
            impl.cmd_push_bindings(cmd, pass, std::span {pass_bindings});
            impl.cmd_dispatch(cmd, pass, uvec3(irradiance_resolution, irradiance_resolution, 6));

            rhi::global_barrier barriers[] {
                rhi::global_barrier {
                                     .before = {.stages = rhi::barrier_stage::compute_shader,
                               .access = rhi::barrier_access::storage_write},
                                     .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::fragment_shader,
                               .access = rhi::barrier_access::sampled_read},
                                     },
            };

            impl.cmd_barriers(cmd, {.globals = std::span {barriers}});
        }

        {
            // TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap prefilter"));

            const rhi::pipeline& pass = pso[pso_id::cubemap_prefilter_pipeline];

            struct push_constants
            {
                i32 resolution;
                i32 env_resolution;
                f32 roughness;
            };

            impl.cmd_bind_pso(cmd, pass);
            i32 mip_resolution = prefilter_resolution;

            for (u32 i = 0; i < shader_constants::kEnvPrefilterMips; ++i)
            {
                const rhi::binding pass_bindings[] = {
                    rhi::binding(cube_view, sampler),
                    rhi::binding(pref_mips[i]),
                };

                const push_constants pc {.resolution     = mip_resolution,
                                         .env_resolution = env_resolution,
                                         .roughness      = static_cast<f32>(i)
                                                    / static_cast<f32>(shader_constants::kEnvPrefilterMips - 1)};

                impl.cmd_push_constants(cmd, pass, &pc, sizeof(pc), 0);
                impl.cmd_push_bindings(cmd, pass, std::span {pass_bindings});
                impl.cmd_dispatch(cmd, pass, uvec3(mip_resolution, mip_resolution, 6));

                mip_resolution >>= 1;
                rhi::global_barrier barriers[] {
                    rhi::global_barrier {
                                         .before = {.stages = rhi::barrier_stage::compute_shader,
                                   .access = rhi::barrier_access::storage_write},
                                         .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::fragment_shader,
                                   .access = rhi::barrier_access::sampled_read},
                                         },
                };

                impl.cmd_barriers(cmd, {.globals = std::span {barriers}});
            }
        }
        return;
    }

    LOG_WARNING("Failed to load environment map at {}", path.c_str());
}
