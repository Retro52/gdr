#include <app/environment.hpp>
#include <app/pso.hpp>
#include <log.hpp>
#include <render/platform/vk/vk_barrier.hpp>
#include <render/platform/vk/vk_buffer_transfer.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <scene/loader.hpp>

app::environment::environment(const render::vk_renderer& renderer, VkFormat format, i32 resolution,
                              i32 irradiance_resolution, i32 prefilter_resolution)
    : resolution(resolution)
    , prefilter_resolution(prefilter_resolution)
    , irradiance_resolution(irradiance_resolution)
{
    VkImageCreateInfo image_create_info {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .mipLevels     = 1,
        .arrayLayers   = 6,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    {
        image_create_info.extent = {static_cast<u32>(resolution), static_cast<u32>(resolution), 1},
        cubemap                  = *render::create_image(renderer.get_context().device,
                                        image_create_info,
                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                        renderer.get_context().allocator);
    }

    {
        image_create_info.extent = {static_cast<u32>(irradiance_resolution),
                                    static_cast<u32>(irradiance_resolution),
                                    1},
        convolution              = *render::create_image(renderer.get_context().device,
                                            image_create_info,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            renderer.get_context().allocator);
    }

    {
        image_create_info.mipLevels = 5;
        image_create_info.extent = {static_cast<u32>(prefilter_resolution), static_cast<u32>(prefilter_resolution), 1},
        prefiltered              = *render::create_image(renderer.get_context().device,
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

void app::environment::shutdown(const render::vk_renderer& renderer)
{
    vkDestroySampler(renderer.get_context().device, sampler, nullptr);
    vkDestroyImageView(renderer.get_context().device, cube_view, nullptr);
    vkDestroyImageView(renderer.get_context().device, conv_view, nullptr);
    vkDestroyImageView(renderer.get_context().device, pref_view, nullptr);
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, cubemap);
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, convolution);
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, prefiltered);
}

[[nodiscard]] bool app::environment::valid() const
{
    return irradiance_resolution.get_flag(0);
}

render::vk_descriptor_info app::environment::get_cube_descriptor_info() const
{
    return valid() ? render::vk_descriptor_info(sampler, cube_view, VK_IMAGE_LAYOUT_GENERAL)
                   : render::vk_descriptor_info(sampler, cube_view, VK_IMAGE_LAYOUT_UNDEFINED);
}

render::vk_descriptor_info app::environment::get_conv_descriptor_info() const
{
    return valid() ? render::vk_descriptor_info(sampler, conv_view, VK_IMAGE_LAYOUT_GENERAL)
                   : render::vk_descriptor_info(sampler, conv_view, VK_IMAGE_LAYOUT_UNDEFINED);
}

void app::environment::load(const fs::path& path, app::pso_data& pso, render::vk_renderer& renderer,
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
                render::transition_image(cmd, cubemap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
                render::transition_image(cmd, convolution.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "unpack equirect"));

                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        render::vk_descriptor_info(sampler, equirect->view, VK_IMAGE_LAYOUT_GENERAL),
                        render::vk_descriptor_info(VK_NULL_HANDLE, cubemap.view, VK_IMAGE_LAYOUT_GENERAL),
                    };

                    const render::vk_pipeline& pass = pso[pso_id::equirect_unpack_pipeline];

                    pass.bind(cmd);
                    pass.push_descriptor_set(cmd, cull_pass_bindings);

                    pass.dispatch(cmd, resolution, resolution, 6);

                    render::cmd_stage_barrier(
                        cmd,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
                            | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }

                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), cmd, "cubemap convolution"));

                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        render::vk_descriptor_info(sampler, cube_view, VK_IMAGE_LAYOUT_GENERAL),
                        render::vk_descriptor_info(VK_NULL_HANDLE, convolution.view, VK_IMAGE_LAYOUT_GENERAL),
                    };

                    const render::vk_pipeline& pass = pso[pso_id::cubemap_convolute_pipeline];

                    pass.bind(cmd);
                    pass.push_descriptor_set(cmd, cull_pass_bindings);

                    pass.dispatch(cmd, irradiance_resolution, irradiance_resolution, 6);

                    render::cmd_stage_barrier(
                        cmd,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
                            | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }
            });
        irradiance_resolution.set_flag(0, true);
        return;
    }

    LOG_WARNING("Failed to load environment map at {}", path.c_str());
}
