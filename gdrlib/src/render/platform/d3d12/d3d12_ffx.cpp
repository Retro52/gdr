#if defined(_WIN32)

#include <ffx_upscale.hpp>
#include <log.hpp>
#include <render/platform/d3d12/d3d12_ffx.hpp>
#include <render/platform/d3d12/d3d12_utils.hpp>
#include <tracy/Tracy.hpp>

// clang-format off
#include <dx12/ffx_api_dx12.hpp>
// clang-format on

FfxApiResource platform::d3d12_ffx_get_resource_wrapper(const d3d12_image& image)
{
    ZoneScoped;
    return ffxApiGetResourceDX12(image.resource.Get(), FFX_API_RESOURCE_STATE_COMMON);
}

result<ffx::Context> platform::d3d12_ffx_create_upscale_context(platform::ffx_module& module, ID3D12Device* device,
                                                                const rhi::ffx_upscale_desc& desc)
{
    ZoneScoped;
    module.lazy_load("./amd_fidelityfx_loader_dx12.dll");

    ffx::Context ctx {};
    ffx::CreateBackendDX12Desc backend_desc {};
    backend_desc.device = device;

    ffx::CreateContextDescUpscale upscale_desc {};

    upscale_desc.maxUpscaleSize = {.width = desc.display_size.x, .height = desc.display_size.y};
    upscale_desc.maxRenderSize  = {.width = desc.max_render_size.x, .height = desc.max_render_size.y};
    upscale_desc.flags =
        FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE | FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
#if 1
    upscale_desc.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
    upscale_desc.fpMessage = [](const uint32_t type, const wchar_t* msg)
    {
        switch (type)
        {
        case FFX_API_MESSAGE_TYPE_ERROR :
            LOG_ERROR("FFX Error: {}", platform::d3d12_wchar_to_utf8(msg).c_str());
            break;
        default :
        case FFX_API_MESSAGE_TYPE_WARNING :
            LOG_WARNING("FFX Warning: {}", platform::d3d12_wchar_to_utf8(msg).c_str());
            break;
        }
    };
#endif

    ffx::CreateContextDescUpscaleVersion version_desc {};
    version_desc.version = FFX_UPSCALER_VERSION;

    FFX_RETURN_ON_FAIL(module.CreateContext(ctx, nullptr, upscale_desc, backend_desc, version_desc));

    ffxQueryGetProviderVersion version_query = {};
    version_query.header.type                = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;

    FFX_ASSERT_ON_FAIL(module.Query(ctx, version_query));
    LOG_INFO("FSR version: {}", version_query.versionName);

    return ctx;
}

result<vec2> platform::d3d12_ffx_get_jitter_offset(const platform::ffx_module& module, ffx::Context context,
                                                   const u32 render_width, const u32 display_width,
                                                   const i32 jitter_index)
{
    ZoneScoped;
    i32 jitter_phase_size;
    ffx::QueryDescUpscaleGetJitterPhaseCount get_jitter_phase_desc {};
    get_jitter_phase_desc.renderWidth    = render_width;
    get_jitter_phase_desc.displayWidth   = display_width;
    get_jitter_phase_desc.pOutPhaseCount = &jitter_phase_size;

    FFX_RETURN_ON_FAIL(module.Query(context, get_jitter_phase_desc));

    vec2 result;
    ffx::QueryDescUpscaleGetJitterOffset get_current_jitter_desc {};
    get_current_jitter_desc.index      = jitter_index;
    get_current_jitter_desc.phaseCount = jitter_phase_size;
    get_current_jitter_desc.pOutX      = &result.x;
    get_current_jitter_desc.pOutY      = &result.y;

    FFX_RETURN_ON_FAIL(module.Query(context, get_current_jitter_desc));

    return result;
}

void platform::d3d12_ffx_dispatch_upscale(const platform::ffx_module& module, ffx::Context context,
                                          ID3D12CommandList* cmd, const rhi::ffx_upscale_params& params,
                                          const platform::ffx_upscale_resources& desc)
{
    ZoneScoped;
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

    FFX_ASSERT_ON_FAIL(module.Dispatch(context, dispatch_desc));
}

#endif
