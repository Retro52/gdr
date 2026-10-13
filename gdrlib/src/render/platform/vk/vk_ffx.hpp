#pragma once

#include <volk.h>

#include <types.hpp>

#include <cpp/f16.hpp>
#include <render/platform/ffx/ffx_types.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/types.hpp>
#include <result.hpp>

namespace platform
{
    FfxApiResource vk_ffx_get_resource_wrapper(const vk_image& image);

    FfxApiResourceState vk_ffx_get_resource_state(VkImageLayout layout);

    result<vec2> vk_ffx_get_jitter_offset(ffx::Context context, u32 render_width, u32 display_width, i32 jitter_index);

    result<ffx::Context> vk_ffx_create_upscale_context(VkDevice device, VkPhysicalDevice physical_device,
                                                       const rhi::ffx_upscale_desc& desc);

    void vk_ffx_dispatch_upscale(ffx::Context context, VkCommandBuffer cmd, const rhi::ffx_upscale_params& params,
                                 const platform::ffx_upscale_resources& desc);

}
