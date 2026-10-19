#pragma once

#include <volk.h>

#include <types.hpp>

#include <cpp/f16.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>
#include <render/platform/ffx/ffx_types.hpp>
#include <render/types.hpp>
#include <result.hpp>

namespace platform
{
    FfxApiResource d3d12_ffx_get_resource_wrapper(const d3d12_image& image);

    FfxApiResourceState d3d12_ffx_get_resource_state(D3D12_BARRIER_LAYOUT layout);

    result<vec2> d3d12_ffx_get_jitter_offset(ffx::Context context, u32 render_width, u32 display_width,
                                             i32 jitter_index);

    result<ffx::Context> d3d12_ffx_create_upscale_context(ID3D12Device* device, const rhi::ffx_upscale_desc& desc);

    void d3d12_ffx_dispatch_upscale(ffx::Context context, ID3D12CommandList* cmd, const rhi::ffx_upscale_params& params,
                                    const platform::ffx_upscale_resources& desc);

}
