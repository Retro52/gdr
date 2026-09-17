#include <log.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_queue.hpp>
#include <tracy/Tracy.hpp>

// FIXME: extract into a utility
static cpp::big_stack_string to_utf8(const wchar_t* wide)
{
    ZoneScoped;
    cpp::big_stack_string buf;
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, buf.data(), cpp::stack_string::capacity(), nullptr, nullptr);
    return cpp::big_stack_string::make_formatted("%s", buf);
}

static cpp::stack_string format_bytes_number(u64 number)
{
    ZoneScoped;
    if (number < 1024)
    {
        return cpp::stack_string::make_formatted("%llu.000 B", number);
    }

    const char* magnitudes_per_thousand[] = {"KB", "MB", "GB", "TB"};

    auto magnitude     = static_cast<i32>(std::log2(number) / 10);
    const f64 fraction = static_cast<f64>(number) / std::pow(1024, magnitude);
    return cpp::stack_string::make_formatted("%.3lf %s", fraction, magnitudes_per_thousand[magnitude - 1]);
}

static const char* shader_model_ver_to_str(const D3D_SHADER_MODEL sm)
{
    switch (sm)
    {
    case D3D_SHADER_MODEL_6_8 :
        return "6.8";
    case D3D_SHADER_MODEL_6_7 :
        return "6.7";
    case D3D_SHADER_MODEL_6_6 :
        return "6.6";
    case D3D_SHADER_MODEL_6_5 :
        return "6.5";
    case D3D_SHADER_MODEL_6_4 :
        return "6.4";
    case D3D_SHADER_MODEL_6_3 :
        return "6.3";
    case D3D_SHADER_MODEL_6_2 :
        return "6.2";
    case D3D_SHADER_MODEL_6_1 :
        return "6.1";
    case D3D_SHADER_MODEL_6_0 :
        return "6.0";
    default :
        return "5.1";
    }
}

static const char* feature_level_ver_to_str(const D3D_FEATURE_LEVEL fl)
{
    switch (fl)
    {
    case D3D_FEATURE_LEVEL_12_0 :
        return "12.0";
    case D3D_FEATURE_LEVEL_12_1 :
        return "12.1";
    case D3D_FEATURE_LEVEL_12_2 :
        return "12.2";
    default :
    case D3D_FEATURE_LEVEL_11_1 :
        return "11.1";
    }
}

static const char* debug_message_category_to_str(const D3D12_MESSAGE_CATEGORY category)
{
    switch (category)
    {
    case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED :
        return "APPLICATION_DEFINED";
    case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS :
        return "MISCELLANEOUS";
    case D3D12_MESSAGE_CATEGORY_INITIALIZATION :
        return "INITIALIZATION";
    case D3D12_MESSAGE_CATEGORY_CLEANUP :
        return "CLEANUP";
    case D3D12_MESSAGE_CATEGORY_COMPILATION :
        return "COMPILATION";
    case D3D12_MESSAGE_CATEGORY_STATE_CREATION :
        return "STATE_CREATION";
    case D3D12_MESSAGE_CATEGORY_STATE_SETTING :
        return "STATE_SETTING";
    case D3D12_MESSAGE_CATEGORY_STATE_GETTING :
        return "STATE_GETTING";
    case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION :
        return "RESOURCE_MANIPULATION";
    case D3D12_MESSAGE_CATEGORY_EXECUTION :
        return "EXECUTION";
    case D3D12_MESSAGE_CATEGORY_SHADER :
        return "SHADER";
    default :
        return "[UNKNOWN]";
    }
}

static void log_device_features_table(const char* name, const D3D_FEATURE_LEVEL feature_level,
                                      const D3D_SHADER_MODEL shader_model,
                                      const render::rhi::rendering_features_table& feat_table)
{
    ZoneScoped;
    LOG_DEBUG("{} features report:", name);
    LOG_DEBUG("Shader model: {}", shader_model_ver_to_str(shader_model));
    LOG_DEBUG("Feature level: {}", feature_level_ver_to_str(feature_level));
    for (u32 i = 0; i < reflection::get_enum_values_count<render::rhi::feature_flag>() - 1; ++i)
    {
        const auto flag = reflection::get_enum_value_at<render::rhi::feature_flag>(i);

        // validation is an instance-level flag tbh
        if (flag == render::rhi::feature_flag::eValidation)
        {
            continue;
        }

        const bool supported = feat_table.supported(flag);
        const bool wanted    = feat_table.requested(flag) || feat_table.required(flag);

        if (supported || !wanted)
        {
            LOG_DEBUG("{}: {}",
                      reflection::get_enum_name_at<render::rhi::feature_flag>(i),
                      supported ? "supported" : "unsupported");
        }
        else if (wanted)
        {
            LOG_WARNING("{}: feature requested but unsupported",
                        reflection::get_enum_name_at<render::rhi::feature_flag>(i));
        }
    }
}

static void enable_debug_layer()
{
    ZoneScoped;

    render::com_ptr<ID3D12Debug> debug_interface;
    D3D12_ASSERT_ON_FAIL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
    debug_interface->EnableDebugLayer();
}

static void message_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity,
                             D3D12_MESSAGE_ID /* id */, LPCSTR message, void* /* context */)
{
    if (!message)
    {
        return;
    }

    switch (severity)
    {
    case D3D12_MESSAGE_SEVERITY_ERROR :
    case D3D12_MESSAGE_SEVERITY_CORRUPTION :
        LOG_ERROR("validation error ({}): {}", debug_message_category_to_str(category), message);
        assert2m(false, message);
        break;
    case D3D12_MESSAGE_SEVERITY_WARNING :
        LOG_WARNING("validation warning ({}): {}", debug_message_category_to_str(category), message);
        break;
    case D3D12_MESSAGE_SEVERITY_INFO :
    case D3D12_MESSAGE_SEVERITY_MESSAGE :
        LOG_INFO("validation message ({}): {}", debug_message_category_to_str(category), message);
        break;
    default :
        break;
    }
}

static void create_debug_layer(render::d3d12_context& context)
{
    ZoneScoped;

    if (SUCCEEDED(context.device.As(&context.info_queue)))
    {
        D3D12_ASSERT_ON_FAIL(context.info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
        D3D12_ASSERT_ON_FAIL(context.info_queue->RegisterMessageCallback(
            message_callback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &context.info_queue_cookie));
    }
}

static D3D_SHADER_MODEL query_shader_model(ID3D12Device* device)
{
    ZoneScoped;
    constexpr D3D_SHADER_MODEL models[] = {
        D3D_SHADER_MODEL_6_8,
        D3D_SHADER_MODEL_6_7,
        D3D_SHADER_MODEL_6_6,
        D3D_SHADER_MODEL_6_5,
        D3D_SHADER_MODEL_6_4,
        D3D_SHADER_MODEL_6_3,
        D3D_SHADER_MODEL_6_2,
        D3D_SHADER_MODEL_6_1,
        D3D_SHADER_MODEL_6_0,
    };

    for (const auto sm : models)
    {
        D3D12_FEATURE_DATA_SHADER_MODEL data {.HighestShaderModel = sm};
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &data, sizeof(data))))
        {
            return data.HighestShaderModel;
        }
    }
    return D3D_SHADER_MODEL_5_1;
}

static D3D_FEATURE_LEVEL query_feature_level(ID3D12Device* device)
{
    ZoneScoped;
    constexpr D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS data {};
    data.NumFeatureLevels        = _countof(feature_levels);
    data.pFeatureLevelsRequested = feature_levels;

    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &data, sizeof(data))))
    {
        return data.MaxSupportedFeatureLevel;
    }
    return D3D_FEATURE_LEVEL_11_1;
}

static bool check_device_features(ID3D12Device* device, const D3D_SHADER_MODEL device_shader_model,
                                  render::rhi::rendering_features_table& wanted_features)
{
    ZoneScoped;
    D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12_options0 {};
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 d3d12_options4 {};
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 d3d12_options7 {};
    D3D12_FEATURE_DATA_D3D12_OPTIONS12 d3d12_options12 {};

    const bool is_device_suitable =
        SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12_options0, sizeof(d3d12_options0)))
        && SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &d3d12_options4, sizeof(d3d12_options4)))
        && SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &d3d12_options7, sizeof(d3d12_options7)))
        && SUCCEEDED(
            device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &d3d12_options12, sizeof(d3d12_options12)));

    if (!is_device_suitable)
    {
        return false;
    }

    // shading-level feature anyway, so I just need to make sure to request proper SM
    // FIXME: I think DirectX doesn't support some draw parameters, need to check
    wanted_features.set_supported(render::rhi::feature_flag::eDrawIndirect, true);
    wanted_features.set_supported(render::rhi::feature_flag::eDynamicRender, true);
    wanted_features.set_supported(render::rhi::feature_flag::eSamplerMinMax, true);
    wanted_features.set_supported(render::rhi::feature_flag::ePipelineStats, true);
    wanted_features.set_supported(render::rhi::feature_flag::ePortabilitySubset, false);

    // not directly portable afaik
    wanted_features.set_supported(render::rhi::feature_flag::e8BitIntegers, true);
    wanted_features.set_supported(render::rhi::feature_flag::eScalarBlockLayout, true);

    wanted_features.set_supported(render::rhi::feature_flag::eMeshShading,
                                  device_shader_model >= D3D_SHADER_MODEL_6_5
                                      && d3d12_options7.MeshShaderTier > D3D12_MESH_SHADER_TIER_NOT_SUPPORTED);

    wanted_features.set_supported(render::rhi::feature_flag::eSynchronization2,
                                  d3d12_options12.EnhancedBarriersSupported);

    wanted_features.set_supported(render::rhi::feature_flag::e16BitTypes,
                                  device_shader_model >= D3D_SHADER_MODEL_6_2
                                      && d3d12_options4.Native16BitShaderOpsSupported);

    wanted_features.set_supported(render::rhi::feature_flag::eBindlessTextures,
                                  device_shader_model >= D3D_SHADER_MODEL_6_6
                                      && d3d12_options0.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3);

    return wanted_features.all_required_supported();
}

static HRESULT pick_and_create_device(render::d3d12_context& ctx,
                                      const render::rhi::rendering_features_table& wanted_features,
                                      const u32 device_hint_id)
{
    ZoneScoped;

    render::com_ptr<IDXGIAdapter4> adapter;
    for (UINT i = 0; SUCCEEDED(
             ctx.factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
         ++i)
    {
        DXGI_ADAPTER_DESC3 desc {};
        adapter->GetDesc3(&desc);

        const auto name = to_utf8(desc.Description);

        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
        {
            LOG_DEBUG("Device #{} ({}) skipped due to software impl. flag", i, name.c_str());
            continue;
        }

        LOG_DEBUG("Evaluating device #{}: {} (VRAM budget: {})",
                  i,
                  name.c_str(),
                  format_bytes_number(desc.DedicatedVideoMemory).c_str());

        render::com_ptr<ID3D12Device10> device;
        {
            ZoneScopedN("D3D12CreateDevice");
            if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device))))
            {
                LOG_DEBUG("Device #{} does not support the minimum feature level (12.0)", i);
                continue;
            }
        }

        auto table               = wanted_features;
        const auto shader_model  = query_shader_model(device.Get());
        const auto feature_level = query_feature_level(device.Get());
        if (!check_device_features(device.Get(), shader_model, table))
        {
            LOG_DEBUG("Device #{} does not support all required features", i);
            log_device_features_table(name.c_str(), feature_level, shader_model, table);
            continue;
        }

        const bool force_select = i == device_hint_id;
        if (!ctx.device || force_select)
        {
            ctx.adapter                 = adapter;
            ctx.device                  = device;
            ctx.enabled_device_features = table;
            ctx.shader_model            = shader_model;
            ctx.feature_level           = feature_level;
        }

        if (force_select)
        {
            LOG_DEBUG("Device #{} selected according to device id hint", i);
            break;
        }
    }

    if (ctx.device)
    {
        DXGI_ADAPTER_DESC3 desc {};
        ctx.adapter->GetDesc3(&desc);
        const auto name = to_utf8(desc.Description);
        LOG_INFO("Selected device: {}", name.c_str());
        log_device_features_table(name.c_str(), ctx.feature_level, ctx.shader_model, ctx.enabled_device_features);

        if (ctx.enabled_device_features.requested(render::rhi::feature_flag::eValidation))
        {
            create_debug_layer(ctx);
        }
    }

    return ctx.device != nullptr ? NO_ERROR : DXGI_ERROR_INVALID_CALL;
}

void render::d3d12_destroy_context(d3d12_context& ctx)
{
    ZoneScoped;

    ctx.allocator.Reset();
    for (auto& queue : ctx.queues)
    {
        queue.Reset();
    }

    if (ctx.info_queue)
    {
        ctx.info_queue->UnregisterMessageCallback(ctx.info_queue_cookie);

        ctx.info_queue.Reset();
        ctx.info_queue_cookie = 0;
    }

    ctx.device.Reset();
    ctx.factory.Reset();
    ctx.adapter.Reset();

    ctx.window_handle = nullptr;

    if (ctx.enabled_device_features.requested(render::rhi::feature_flag::eValidation))
    {
        com_ptr<IDXGIDebug1> dxgi_debug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug))))
        {
            D3D12_ASSERT_ON_FAIL(dxgi_debug->ReportLiveObjects(
                DXGI_DEBUG_ALL,
                static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)));
        }
    }
}

auto render::d3d12_create_context(const window& window, const rhi::instance_desc& desc) -> result<d3d12_context>
{
    ZoneScoped;
    d3d12_context context;

    const bool enable_validation = desc.device_features.requested(render::rhi::feature_flag::eValidation);
    if (enable_validation)
    {
        enable_debug_layer();
    }

    const UINT factory_flags = enable_validation ? DXGI_CREATE_FACTORY_DEBUG : 0;
    D3D12_RETURN_ON_FAIL(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&context.factory)));

    context.window_handle = static_cast<HWND>(window.get_native_handle().windows.hwnd);
    D3D12_RETURN_ON_FAIL(pick_and_create_device(context, desc.device_features, desc.device_id_hint));

    const D3D12MA::ALLOCATOR_DESC allocator_desc {
        .Flags              = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
        .pDevice            = context.device.Get(),
        .PreferredBlockSize = 0,  // i.e. default?
        .pAdapter           = context.adapter.Get(),
    };
    D3D12_RETURN_ON_FAIL(D3D12MA::CreateAllocator(&allocator_desc, &context.allocator));

    const auto queue_copy    = create_queue(context, D3D12_COMMAND_LIST_TYPE_COPY);
    const auto queue_direct  = create_queue(context, D3D12_COMMAND_LIST_TYPE_DIRECT);
    const auto queue_compute = create_queue(context, D3D12_COMMAND_LIST_TYPE_COMPUTE);

    RESULT_FORWARD_IF_FAILED(queue_copy);
    RESULT_FORWARD_IF_FAILED(queue_direct);
    RESULT_FORWARD_IF_FAILED(queue_compute);

    context.queues[static_cast<u32>(render::rhi::queue_kind::eGfx)]      = *queue_direct;
    context.queues[static_cast<u32>(render::rhi::queue_kind::ePresent)]  = *queue_direct;
    context.queues[static_cast<u32>(render::rhi::queue_kind::eCompute)]  = *queue_compute;
    context.queues[static_cast<u32>(render::rhi::queue_kind::eTransfer)] = *queue_copy;

    return context;
}

void render::d3d12_destroy_swapchain(const d3d12_context& /* d3d12_context */, d3d12_swapchain& swapchain)
{
    ZoneScoped;
    swapchain.images.clear();
    swapchain.swapchain.Reset();
}

auto render::d3d12_create_swapchain(const d3d12_context& d3d12_context, const u32 format, const ivec2 size,
                                    const u32 frames_in_flight, const bool vsync) -> result<d3d12_swapchain>
{
    ZoneScoped;

    BOOL tearing_supported = FALSE;
    if (FAILED(d3d12_context.factory->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing_supported, sizeof(tearing_supported))))
    {
        tearing_supported = FALSE;
    }

    const DXGI_SWAP_CHAIN_DESC1 desc {
        .Width       = static_cast<UINT>(size.x),
        .Height      = static_cast<UINT>(size.y),
        .Format      = static_cast<DXGI_FORMAT>(format),
        .Stereo      = FALSE,
        .SampleDesc  = {1, 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = frames_in_flight,
        .Scaling     = DXGI_SCALING_NONE,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags       = (!vsync && tearing_supported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U
    };

    com_ptr<IDXGISwapChain1> swapchain;
    D3D12_RETURN_ON_FAIL(d3d12_context.factory->CreateSwapChainForHwnd(
        d3d12_context.queues[static_cast<u32>(render::rhi::queue_kind::ePresent)].Get(),
        d3d12_context.window_handle,
        &desc,
        nullptr,
        nullptr,
        &swapchain));

    d3d12_swapchain result;
    D3D12_RETURN_ON_FAIL(swapchain.As(&result.swapchain));
    D3D12_ASSERT_ON_FAIL(
        d3d12_context.factory->MakeWindowAssociation(d3d12_context.window_handle, DXGI_MWA_NO_ALT_ENTER));

    return result;
}
