#include <ffx_api/ffx_upscale.hpp>
#include <log.hpp>
#include <render/platform/vk/vk_ffx.hpp>

// clang-format off
#include <ffx_api/vk/ffx_api_vk.hpp>
// clang-format on

static PFN_vkVoidFunction VKAPI_PTR vk_ffx_get_device_proc_addr_wrapper(VkDevice device, const char* name)
{
    const auto cpp_name   = cpp::stack_string(name);
    const auto cpp_ending = cpp_name.substring(cpp_name.length() - 3, 3);
    if (cpp_ending != "KHR" && cpp_ending != "EXT")
    {
        return vkGetDeviceProcAddr(device, name);
    }

    const auto core_test_name = cpp_name.substring(0, cpp_name.length() - 3);
    if (const auto core_fn = vkGetDeviceProcAddr(device, core_test_name.c_str()))
    {
        return core_fn;
    }

    return vkGetDeviceProcAddr(device, name);
}

FfxApiResource platform::vk_ffx_get_resource_wrapper(const vk_image& image)
{
    const VkImageCreateInfo create_info {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags       = image.meta.layers_count == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0U,
        .imageType   = static_cast<VkImageType>(image.meta.type),
        .format      = image.meta.format,
        .extent      = {.width  = static_cast<u32>(image.meta.dimensions.x),
                        .height = static_cast<u32>(image.meta.dimensions.y),
                        .depth  = static_cast<u32>(image.meta.dimensions.z)},
        .mipLevels   = static_cast<u32>(image.meta.mips_count),
        .arrayLayers = static_cast<u32>(image.meta.layers_count),
        .usage       = image.meta.usage,
    };

    return ffxApiGetResourceVK(image.image,
                               ffxApiGetImageResourceDescriptionVK(image.image, create_info, 0),
                               vk_ffx_get_resource_state(image.meta.layout));
}

FfxApiResourceState platform::vk_ffx_get_resource_state(const VkImageLayout layout)
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
        return FFX_API_RESOURCE_STATE_RENDER_TARGET;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
        return FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL :
        return FFX_API_RESOURCE_STATE_COPY_SRC;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL :
        return FFX_API_RESOURCE_STATE_COPY_DEST;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR :
        return FFX_API_RESOURCE_STATE_PRESENT;
    default :
    case VK_IMAGE_LAYOUT_GENERAL :
        return FFX_API_RESOURCE_STATE_COMMON;
    }
}

result<vec2> platform::vk_ffx_get_jitter_offset(ffx::Context context, const u32 render_width, const u32 display_width,
                                                const i32 jitter_index)
{
    i32 jitter_phase_size;
    ffx::QueryDescUpscaleGetJitterPhaseCount get_jitter_phase_desc {};
    get_jitter_phase_desc.renderWidth    = render_width;
    get_jitter_phase_desc.displayWidth   = display_width;
    get_jitter_phase_desc.pOutPhaseCount = &jitter_phase_size;

    FFX_RETURN_ON_FAIL(ffx::Query(context, get_jitter_phase_desc));

    vec2 result;
    ffx::QueryDescUpscaleGetJitterOffset get_current_jitter_desc {};
    get_current_jitter_desc.index      = jitter_index;
    get_current_jitter_desc.phaseCount = jitter_phase_size;
    get_current_jitter_desc.pOutX      = &result.x;
    get_current_jitter_desc.pOutY      = &result.y;

    FFX_RETURN_ON_FAIL(ffx::Query(context, get_current_jitter_desc));

    return result;
}

result<ffx::Context> platform::vk_ffx_create_upscale_context(VkDevice device, VkPhysicalDevice physical_device,
                                                             const rhi::ffx_upscale_desc& desc)
{
    ffx::Context ctx {};
    ffx::CreateBackendVKDesc backend_desc {};
    backend_desc.vkDevice         = device;
    backend_desc.vkPhysicalDevice = physical_device;
    backend_desc.vkDeviceProcAddr = vk_ffx_get_device_proc_addr_wrapper;

    ffx::CreateContextDescUpscale upscale_desc {};

    upscale_desc.maxUpscaleSize = {.width = desc.display_size.x, .height = desc.display_size.y};
    upscale_desc.maxRenderSize  = {.width = desc.max_render_size.x, .height = desc.max_render_size.y};
    upscale_desc.flags =
        FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE | FFX_UPSCALE_ENABLE_DEPTH_INVERTED;

#if 1
    upscale_desc.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING;  // debug builds
    upscale_desc.fpMessage = [](uint32_t type, const wchar_t* msg)
    {
        assert2(false && msg);
    };
#endif

    FFX_RETURN_ON_FAIL(ffx::CreateContext(ctx, nullptr, upscale_desc, backend_desc));
    return ctx;
}

void platform::vk_ffx_dispatch_upscale(ffx::Context context, VkCommandBuffer cmd, const rhi::ffx_upscale_params& params,
                                       const platform::ffx_upscale_resources& desc)
{
    ffx::DispatchDescUpscale dispatch_desc {};

    dispatch_desc.commandList = cmd;

    dispatch_desc.preExposure                = 1.0f;
    dispatch_desc.color                      = desc.color;
    dispatch_desc.depth                      = desc.depth;
    dispatch_desc.motionVectors              = desc.motion_vectors;
    dispatch_desc.output                     = desc.output;
    dispatch_desc.reactive                   = desc.reactive;
    dispatch_desc.transparencyAndComposition = desc.transparency_and_composition;

    // Jitter is calculated earlier in the frame using a callback from the camera update
    dispatch_desc.jitterOffset.x      = -params.jitter.x;
    dispatch_desc.jitterOffset.y      = -params.jitter.y;
    dispatch_desc.motionVectorScale.x = static_cast<f32>(params.render_size.x);
    dispatch_desc.motionVectorScale.y = static_cast<f32>(params.render_size.y);
    dispatch_desc.reset               = params.reset;
    dispatch_desc.enableSharpening    = params.enable_sharpness;
    dispatch_desc.sharpness           = params.sharpness;

    // Cauldron keeps time in seconds, but FSR expects milliseconds
    dispatch_desc.frameTimeDelta = params.dt_ms;

    // dispatch_desc.preExposure        = GetScene()->GetSceneExposure();
    dispatch_desc.renderSize.width  = params.render_size.x;
    dispatch_desc.renderSize.height = params.render_size.y;

    dispatch_desc.upscaleSize.width  = params.display_size.x;
    dispatch_desc.upscaleSize.height = params.display_size.y;

    // Setup camera params as required
    dispatch_desc.cameraFovAngleVertical = params.camera_fov_radians;

    if (params.reverse_z)
    {
        dispatch_desc.cameraFar  = params.camera_near_plane;
        dispatch_desc.cameraNear = params.camera_far_plane;
    }
    else
    {
        dispatch_desc.cameraFar  = params.camera_far_plane;
        dispatch_desc.cameraNear = params.camera_near_plane;
    }

    FFX_ASSERT_ON_FAIL(ffx::Dispatch(context, dispatch_desc));
}
