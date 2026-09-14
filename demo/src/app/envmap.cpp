#include <app/envmap.hpp>
#include <app/pso.hpp>
#include <log.hpp>
#include <render/platform/vk/vk_barrier.hpp>
#include <render/platform/vk/vk_buffer_transfer.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <scene/loader.hpp>

static u32 get_mips_count(i32 resolution)
{
    return 1u + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(resolution))));
}

static void generate_mips(VkCommandBuffer cmd, render::vk_image& cube, i32 resolution)
{
    u32 mips = get_mips_count(resolution);
    for (u32 i = 1; i < mips; ++i)
    {
        i32 mip_resolution = resolution > 1 ? resolution / 2 : resolution;

        VkImageBlit blit {
            .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 6},
            .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i,     0, 6}
        };
        blit.srcOffsets[1] = {resolution, resolution, 1};
        blit.dstOffsets[1] = {mip_resolution, mip_resolution, 1};

        vkCmdBlitImage(
            cmd, cube.image, VK_IMAGE_LAYOUT_GENERAL, cube.image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);

        render::transition_image(cmd,
                                 cube.image,
                                 VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_LAYOUT_GENERAL,
                                 VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                 VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                 VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);

        resolution = mip_resolution;
    }
}

static void make_brdf_lut(VkCommandBuffer cmd, const render::vk_pipeline& pso, const render::vk_image& brdf_lut,
                          i32 resolution)
{
    const render::vk_descriptor_info bindings[] = {
        render::vk_descriptor_info(VK_NULL_HANDLE, brdf_lut.view, VK_IMAGE_LAYOUT_GENERAL),
    };

    pso.bind(cmd);
    pso.push_constant(cmd, resolution);
    pso.push_descriptor_set(cmd, bindings);
    pso.dispatch(cmd, resolution, resolution, 1);

    render::cmd_stage_barrier(cmd,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

app::envmap::envmap(const render::vk_renderer& renderer, VkFormat format, const envmap_config& cfg)
    : env_resolution(cfg.env_resolution)
    , brdf_lut_resolution(cfg.brdf_lut_resolution)
    , prefilter_resolution(cfg.prefilter_resolution)
    , irradiance_resolution(cfg.irradiance_resolution)
{
    VkImageCreateInfo image_create_info {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags       = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = format,
        .arrayLayers = 6,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
               | VK_IMAGE_USAGE_STORAGE_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    {
        image_create_info.mipLevels = get_mips_count(env_resolution);
        image_create_info.extent    = {static_cast<u32>(env_resolution), static_cast<u32>(env_resolution), 1},
        cubemap                     = *render::create_image(renderer.get_context().device,
                                        image_create_info,
                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                        renderer.get_context().allocator);
    }

    {
        image_create_info.mipLevels = 1;
        image_create_info.extent    = {static_cast<u32>(irradiance_resolution),
                                       static_cast<u32>(irradiance_resolution),
                                       1},
        convolution                 = *render::create_image(renderer.get_context().device,
                                            image_create_info,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            renderer.get_context().allocator);
    }

    {
        image_create_info.mipLevels = shader_constants::kEnvPrefilterMips;
        image_create_info.extent = {static_cast<u32>(prefilter_resolution), static_cast<u32>(prefilter_resolution), 1},
        prefiltered              = *render::create_image(renderer.get_context().device,
                                            image_create_info,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            renderer.get_context().allocator);

        for (u32 i = 0; i < shader_constants::kEnvPrefilterMips; ++i)
        {
            pref_mips[i] = *render::create_image_view(renderer.get_context().device,
                                                      prefiltered.image,
                                                      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                                                      format,
                                                      VK_IMAGE_ASPECT_COLOR_BIT,
                                                      i,
                                                      1);
        }
    }

    {
        image_create_info.flags       = 0;
        image_create_info.mipLevels   = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.format      = VK_FORMAT_R16G16_SFLOAT;
        image_create_info.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,

        image_create_info.extent = {static_cast<u32>(brdf_lut_resolution), static_cast<u32>(brdf_lut_resolution), 1},
        brdf_lut                 = *render::create_image(renderer.get_context().device,
                                         image_create_info,
                                         VK_IMAGE_ASPECT_COLOR_BIT,
                                         renderer.get_context().allocator);
    }

    sampler = *render::create_sampler(renderer.get_context().device,
                                      VK_FILTER_LINEAR,
                                      VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                      VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                      VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE,
                                      16.0F);

    brdf_sampler = *render::create_sampler(renderer.get_context().device,
                                           VK_FILTER_LINEAR,
                                           VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                           VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                           VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE,
                                           16.0F);

    cube_view = *render::create_image_view(renderer.get_context().device,
                                           cubemap.image,
                                           VK_IMAGE_VIEW_TYPE_CUBE,
                                           format,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           0,
                                           VK_REMAINING_MIP_LEVELS);

    conv_view = *render::create_image_view(renderer.get_context().device,
                                           convolution.image,
                                           VK_IMAGE_VIEW_TYPE_CUBE,
                                           format,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           0,
                                           VK_REMAINING_MIP_LEVELS);

    pref_view = *render::create_image_view(renderer.get_context().device,
                                           prefiltered.image,
                                           VK_IMAGE_VIEW_TYPE_CUBE,
                                           format,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           0,
                                           VK_REMAINING_MIP_LEVELS);
}

void app::envmap::shutdown(const render::vk_renderer& renderer)
{
    vkDestroySampler(renderer.get_context().device, sampler, nullptr);
    vkDestroySampler(renderer.get_context().device, brdf_sampler, nullptr);

    vkDestroyImageView(renderer.get_context().device, cube_view, nullptr);
    vkDestroyImageView(renderer.get_context().device, conv_view, nullptr);
    vkDestroyImageView(renderer.get_context().device, pref_view, nullptr);
    for (auto view : pref_mips)
    {
        vkDestroyImageView(renderer.get_context().device, view, nullptr);
    }

    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, cubemap);
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, brdf_lut);
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, convolution);
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, prefiltered);
}

render::vk_descriptor_info app::envmap::get_lut_descriptor_info() const
{
    return {brdf_sampler, brdf_lut.view, VK_IMAGE_LAYOUT_GENERAL};
}

render::vk_descriptor_info app::envmap::get_cube_descriptor_info() const
{
    return {sampler, cube_view, VK_IMAGE_LAYOUT_GENERAL};
}

render::vk_descriptor_info app::envmap::get_conv_descriptor_info() const
{
    return {sampler, conv_view, VK_IMAGE_LAYOUT_GENERAL};
}

render::vk_descriptor_info app::envmap::get_pref_descriptor_info() const
{
    return {sampler, pref_view, VK_IMAGE_LAYOUT_GENERAL};
}

void app::envmap::init(app::pso_data& pso, render::vk_renderer& renderer)
{
    renderer.submit(
        [&](VkCommandBuffer cmd)
        {
            render::transition_image(cmd, cubemap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            render::transition_image(cmd, brdf_lut.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            render::transition_image(cmd, convolution.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            render::transition_image(cmd, prefiltered.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            {
                TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "brdf lut generation"));
                make_brdf_lut(cmd, pso[pso_id::make_brdf_lookup_pipeline], brdf_lut, brdf_lut_resolution);
            }
        });
}

void app::envmap::load(const fs::path& path, app::pso_data& pso, render::vk_renderer& renderer,
                       const render::vk_buffer_transfer& transfer)
{
    if (auto equirect = loader::load_texture(path, renderer, transfer))
    {
        renderer.schedule_delete(
            [equirect](VkDevice device, VmaAllocator allocator)
            {
                render::destroy_image(device, allocator, *equirect);
            });

        renderer.submit(
            [&](VkCommandBuffer cmd)
            {
                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "unpack equirect"));

                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        render::vk_descriptor_info(sampler, equirect->view, VK_IMAGE_LAYOUT_GENERAL),
                        render::vk_descriptor_info(VK_NULL_HANDLE, cubemap.view, VK_IMAGE_LAYOUT_GENERAL),
                    };

                    const render::vk_pipeline& pass = pso[pso_id::equirect_unpack_pipeline];

                    pass.bind(cmd);
                    pass.push_constant(cmd, env_resolution);
                    pass.push_descriptor_set(cmd, cull_pass_bindings);

                    pass.dispatch(cmd, env_resolution, env_resolution, 6);

                    render::cmd_stage_barrier(cmd,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                              VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                              VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
                }

                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap mips generation"));
                    generate_mips(cmd, cubemap, env_resolution);

                    render::cmd_stage_barrier(cmd,
                                              VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                              VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
                }

                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap convolution"));

                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        render::vk_descriptor_info(sampler, cube_view, VK_IMAGE_LAYOUT_GENERAL),
                        render::vk_descriptor_info(VK_NULL_HANDLE, convolution.view, VK_IMAGE_LAYOUT_GENERAL),
                    };

                    const render::vk_pipeline& pass = pso[pso_id::cubemap_convolute_pipeline];

                    int push_constants[] = {irradiance_resolution, env_resolution};

                    pass.bind(cmd);
                    pass.push_constant(cmd, push_constants);
                    pass.push_descriptor_set(cmd, cull_pass_bindings);

                    pass.dispatch(cmd, irradiance_resolution, irradiance_resolution, 6);

                    render::cmd_stage_barrier(cmd,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                                                  | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
                }

                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap prefilter"));

                    const render::vk_pipeline& pass = pso[pso_id::cubemap_prefilter_pipeline];

                    struct push_constants
                    {
                        i32 resolution;
                        i32 env_resolution;
                        f32 roughness;
                    };

                    pass.bind(cmd);

                    i32 mip_resolution = prefilter_resolution;
                    for (u32 i = 0; i < shader_constants::kEnvPrefilterMips; ++i)
                    {
                        const render::vk_descriptor_info cull_pass_bindings[] = {
                            render::vk_descriptor_info(sampler, cube_view, VK_IMAGE_LAYOUT_GENERAL),
                            render::vk_descriptor_info(VK_NULL_HANDLE, pref_mips[i], VK_IMAGE_LAYOUT_GENERAL),
                        };

                        pass.push_descriptor_set(cmd, cull_pass_bindings);

                        pass.push_constant(
                            cmd,
                            push_constants {mip_resolution,
                                            env_resolution,
                                            static_cast<f32>(i)
                                                / static_cast<f32>(shader_constants::kEnvPrefilterMips - 1)});

                        pass.dispatch(cmd, mip_resolution, mip_resolution, 6);
                        render::cmd_stage_barrier(cmd,
                                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                                                      | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

                        mip_resolution >>= 1;
                    }
                }
            });
        return;
    }

    LOG_WARNING("Failed to load environment map at {}", path.c_str());
}
