#include <app/envmap.hpp>
#include <app/pso.hpp>
#include <app/render.hpp>
#include <log.hpp>
#include <scene/loader.hpp>

static u32 get_mips_count(i32 resolution)
{
    return 1u + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(resolution))));
}

static void generate_mips(const render::rhi::rhi& rhi, const render::rhi::command_buffer cmd,
                          const render::rhi::image cube, i32 resolution)
{
    const u32 mips = get_mips_count(resolution);
    for (u32 i = 1; i < mips; ++i)
    {
        const i32 mip_resolution = resolution > 1 ? resolution / 2 : resolution;

        // VkImageBlit blit {
        //     .srcSubresource = {.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        //                        .mipLevel       = i - 1,
        //                        .baseArrayLayer = 0,
        //                        .layerCount     = 6 },
        //     .dstSubresource = {
        //                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,     .mipLevel = i, .baseArrayLayer = 0,
        //                        .layerCount = 6}
        // };
        // blit.srcOffsets[1] = {.x = resolution, .y = resolution, .z = 1};
        // blit.dstOffsets[1] = {.x = mip_resolution, .y = mip_resolution, .z = 1};
        //
        // vkCmdBlitImage(
        //     cmd, cube.image, VK_IMAGE_LAYOUT_GENERAL, cube.image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit,
        //     VK_FILTER_LINEAR);
        //
        // render::vk_transition_image(cmd,
        //                             cube.image,
        //                             VK_IMAGE_LAYOUT_GENERAL,
        //                             VK_IMAGE_LAYOUT_GENERAL,
        //                             VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        //                             VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        //                             VK_ACCESS_2_TRANSFER_WRITE_BIT,
        //                             VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);

        resolution = mip_resolution;
    }
}

static void make_brdf_lut(const render::rhi::rhi& rhi, const render::rhi::command_buffer cmd,
                          const render::rhi::pipeline pso, const render::rhi::image brdf_lut, i32 resolution)
{
    const render::rhi::binding bindings[] = {
        {brdf_lut},
    };

    rhi.cmd_bind_pso(cmd, pso);
    rhi.cmd_push_constants(cmd, pso, &resolution, sizeof(resolution), 0);
    rhi.cmd_push_bindings(cmd, pso, std::span {bindings});
    rhi.cmd_dispatch(cmd, {resolution, resolution, 1});

    render::rhi::global_barrier barrier {
        .before = {.stages = (u32)render::rhi::barrier_stage::compute_shader,
                   .access = (u32)render::rhi::barrier_access::storage_write},
        .after  = {.stages =
                       (u32)render::rhi::barrier_stage::compute_shader | render::rhi::barrier_stage::fragment_shader,
                   .access = (u32)render::rhi::barrier_access::sampled_read },
    };

    rhi.cmd_barriers(cmd,
                     {
                         .globals = std::span {&barrier, 1}
    });
}

app::envmap::envmap(const render::rhi::rhi& rhi, render::rhi::context ctx, render::rhi::image_format format,
                    const envmap_config& cfg)
    : env_resolution(cfg.env_resolution)
    , brdf_lut_resolution(cfg.brdf_lut_resolution)
    , prefilter_resolution(cfg.prefilter_resolution)
    , irradiance_resolution(cfg.irradiance_resolution)

{
    render::rhi::create_image_info image_create_info {
        .format      = format,
        .layer_count = 6,
        .usage_flags = render::rhi::image_usage::transfer_dst | render::rhi::image_usage::transfer_src
                     | render::rhi::image_usage::sampled | render::rhi::image_usage::storage};

    {
        image_create_info.mips_count = get_mips_count(env_resolution);
        image_create_info.dimensions = {env_resolution, env_resolution, 1},
        cubemap                      = *rhi.create_image(ctx, image_create_info);
    }

    {
        image_create_info.mips_count = 1;
        image_create_info.dimensions = {irradiance_resolution, irradiance_resolution, 1},
        convolution                  = *rhi.create_image(ctx, image_create_info);
    }

    {
        image_create_info.mips_count = shader_constants::kEnvPrefilterMips;
        image_create_info.dimensions = {prefilter_resolution, prefilter_resolution, 1},
        prefiltered                  = *rhi.create_image(ctx, image_create_info);

        for (u32 i = 0; i < shader_constants::kEnvPrefilterMips; ++i)
        {
            render::rhi::create_image_view_info image_view_info {
                .range  = {.mips_range = {i, 1}},
                .format = format,
                .kind   = render::rhi::image_view_kind::array_2d,
            };

            pref_mips[i] = *rhi.create_image_view(ctx, prefiltered, image_view_info);
        }
    }

    {
        image_create_info.mips_count  = 1;
        image_create_info.layer_count = 1;
        image_create_info.format      = render::rhi::image_format::r16g16sf;
        image_create_info.usage_flags = render::rhi::image_usage::sampled | render::rhi::image_usage::storage,

        image_create_info.dimensions = {brdf_lut_resolution, brdf_lut_resolution, 1},
        brdf_lut                     = *rhi.create_image(ctx, image_create_info);
    }

    sampler      = *rhi.create_sampler(ctx, {.anisotropy_factor = 16});
    brdf_sampler = *rhi.create_sampler(
        ctx, {.address_mode = render::rhi::sampler_address_mode::clamp_to_edge, .anisotropy_factor = 16});

    cube_view =
        *rhi.create_image_view(ctx, cubemap, {.format = format, .kind = render::rhi::image_view_kind::flat_cube});
    conv_view =
        *rhi.create_image_view(ctx, convolution, {.format = format, .kind = render::rhi::image_view_kind::flat_cube});
    pref_view =
        *rhi.create_image_view(ctx, prefiltered, {.format = format, .kind = render::rhi::image_view_kind::flat_cube});
}

void app::envmap::shutdown(const render::rhi::rhi& rhi, render::rhi::context ctx)
{
    rhi.destroy_sampler(ctx, sampler);
    rhi.destroy_sampler(ctx, brdf_sampler);

    rhi.destroy_image_view(ctx, cube_view);
    rhi.destroy_image_view(ctx, conv_view);
    rhi.destroy_image_view(ctx, pref_view);

    for (auto& view : pref_mips)
    {
        rhi.destroy_image_view(ctx, view);
    }

    rhi.destroy_image(ctx, cubemap);
    rhi.destroy_image(ctx, brdf_lut);
    rhi.destroy_image(ctx, convolution);
    rhi.destroy_image(ctx, prefiltered);
}

render::rhi::binding app::envmap::get_lut_descriptor_info() const
{
    return {brdf_lut, brdf_sampler};
}

render::rhi::binding app::envmap::get_cube_descriptor_info() const
{
    return {cube_view, sampler};
}

render::rhi::binding app::envmap::get_conv_descriptor_info() const
{
    return {conv_view, sampler};
}

render::rhi::binding app::envmap::get_pref_descriptor_info() const
{
    return {pref_view, sampler};
}

void app::envmap::init(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd, app::pso_data& pso)
{
    render::rhi::image_barrier barriers[] {
        app::make_image_barrier(cubemap, render::rhi::image_layout::common),
        app::make_image_barrier(brdf_lut, render::rhi::image_layout::common),
        app::make_image_barrier(convolution, render::rhi::image_layout::common),
        app::make_image_barrier(prefiltered, render::rhi::image_layout::common),
    };

    rhi.cmd_barriers(cmd, {.images = std::span {barriers}});
    make_brdf_lut(rhi, cmd, pso[pso_id::make_brdf_lookup_pipeline], brdf_lut, brdf_lut_resolution);
}

void app::envmap::load(const fs::path& path, app::pso_data& pso, const render::rhi::rhi& rhi, render::rhi::context ctx,
                       const app::gpu_upload_mgr& upload_mgr)
{
    // if (auto equirect = loader::load_texture(path, renderer, transfer))
    // {
    //     renderer.schedule_delete(
    //         [equirect](VkDevice device, VmaAllocator allocator)
    //         {
    //             render::vk_destroy_image(device, allocator, *equirect);
    //         });
    //
    //     renderer.submit(
    //         [&](VkCommandBuffer cmd)
    //         {
    //             {
    //                 TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "unpack equirect"));
    //
    //                 const render::vk_descriptor_info cull_pass_bindings[] = {
    //                     render::vk_descriptor_info(sampler, equirect->view, VK_IMAGE_LAYOUT_GENERAL),
    //                     render::vk_descriptor_info(VK_NULL_HANDLE, cubemap.view, VK_IMAGE_LAYOUT_GENERAL),
    //                 };
    //
    //                 const render::vk_pipeline& pass = pso[pso_id::equirect_unpack_pipeline];
    //
    //                 pass.bind(cmd);
    //                 pass.push_constant(cmd, env_resolution);
    //                 pass.push_descriptor_set(cmd, cull_pass_bindings);
    //
    //                 pass.dispatch(cmd, env_resolution, env_resolution, 6);
    //
    //                 render::vk_stage_barrier(cmd,
    //                                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    //                                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    //                                          VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
    //                                          VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
    //             }
    //
    //             {
    //                 TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap mips generation"));
    //                 generate_mips(cmd, cubemap, env_resolution);
    //
    //                 render::vk_stage_barrier(cmd,
    //                                          VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
    //                                          VK_ACCESS_2_TRANSFER_WRITE_BIT,
    //                                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    //                                          VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    //             }
    //
    //             {
    //                 TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap convolution"));
    //
    //                 const render::vk_descriptor_info cull_pass_bindings[] = {
    //                     render::vk_descriptor_info(sampler, cube_view, VK_IMAGE_LAYOUT_GENERAL),
    //                     render::vk_descriptor_info(VK_NULL_HANDLE, convolution.view, VK_IMAGE_LAYOUT_GENERAL),
    //                 };
    //
    //                 const render::vk_pipeline& pass = pso[pso_id::cubemap_convolute_pipeline];
    //
    //                 int push_constants[] = {irradiance_resolution, env_resolution};
    //
    //                 pass.bind(cmd);
    //                 pass.push_constant(cmd, push_constants);
    //                 pass.push_descriptor_set(cmd, cull_pass_bindings);
    //
    //                 pass.dispatch(cmd, irradiance_resolution, irradiance_resolution, 6);
    //
    //                 render::vk_stage_barrier(cmd,
    //                                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    //                                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    //                                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    //                                              | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    //                                          VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    //             }
    //
    //             {
    //                 TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap prefilter"));
    //
    //                 const render::vk_pipeline& pass = pso[pso_id::cubemap_prefilter_pipeline];
    //
    //                 struct push_constants
    //                 {
    //                     i32 resolution;
    //                     i32 env_resolution;
    //                     f32 roughness;
    //                 };
    //
    //                 pass.bind(cmd);
    //
    //                 i32 mip_resolution = prefilter_resolution;
    //                 for (u32 i = 0; i < shader_constants::kEnvPrefilterMips; ++i)
    //                 {
    //                     const render::vk_descriptor_info cull_pass_bindings[] = {
    //                         render::vk_descriptor_info(sampler, cube_view, VK_IMAGE_LAYOUT_GENERAL),
    //                         render::vk_descriptor_info(VK_NULL_HANDLE, pref_mips[i], VK_IMAGE_LAYOUT_GENERAL),
    //                     };
    //
    //                     pass.push_descriptor_set(cmd, cull_pass_bindings);
    //
    //                     pass.push_constant(
    //                         cmd,
    //                         push_constants {mip_resolution,
    //                                         env_resolution,
    //                                         static_cast<f32>(i)
    //                                             / static_cast<f32>(shader_constants::kEnvPrefilterMips - 1)});
    //
    //                     pass.dispatch(cmd, mip_resolution, mip_resolution, 6);
    //                     render::vk_stage_barrier(cmd,
    //                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    //                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    //                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    //                                                  | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    //                                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    //
    //                     mip_resolution >>= 1;
    //                 }
    //             }
    //         });
    //     return;
    // }
    //
    // LOG_WARNING("Failed to load environment map at {}", path.c_str());
}
