#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_utils.hpp>
#include <render/rhi_d3d12.hpp>
#include <render/rhi_util.hpp>
#include <tracy/Tracy.hpp>

auto render::rhi::d3d12_create_context(const window& window, const instance_desc& desc) -> result<context>
{
    ZoneScoped;
    auto ctx = render::d3d12_create_context(window, desc);

    RESULT_FORWARD_IF_FAILED(ctx);
    return create_handle<context>(*ctx);
}

void render::rhi::d3d12_destroy_context(context& context)
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::d3d12_context>(context))
    {
        render::d3d12_destroy_context(*ctx);
        clear_handle(ctx, context);
    }
}

auto render::rhi::d3d12_create_swapchain(context context, const create_swapchain_info& desc) -> result<swapchain>
{
    ZoneScoped;
    auto* ctx = cast_from_handle<render::d3d12_context>(context);
    if (!ctx)
    {
        return error("Failed to access context");
    }

    auto sc = render::d3d12_create_swapchain(
        *ctx, d3d12_format_from_vk(desc.format), desc.size, desc.frames_in_flight, desc.vsync);

    RESULT_FORWARD_IF_FAILED(sc);
    return create_handle<swapchain>(*sc);
}

auto render::rhi::d3d12_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc)
    -> result<render::rhi::swapchain>
{
    ZoneScoped;
    auto* ctx = cast_from_handle<render::d3d12_context>(context);
    auto* sc  = cast_from_handle<render::d3d12_swapchain>(swapchain);
    if (!ctx || !sc)
    {
        return error("failed to access the context or the swapchain");
    }

    D3D12_RETURN_ON_FAIL(sc->swapchain->ResizeBuffers(desc.frames_in_flight,
                                                      desc.size.x,
                                                      desc.size.y,
                                                      static_cast<DXGI_FORMAT>(d3d12_format_from_vk(desc.format)),
                                                      0));
    return reference_handle<render::rhi::swapchain>(sc);
}

void render::rhi::d3d12_destroy_swapchain(context context, swapchain& swapchain)
{
    ZoneScoped;
    auto* d3d12sc   = cast_from_handle<render::d3d12_swapchain>(swapchain);
    const auto* ctx = cast_from_handle<render::d3d12_context>(context);

    if (ctx && d3d12sc)
    {
        render::d3d12_destroy_swapchain(*ctx, *d3d12sc);
        clear_handle(d3d12sc, swapchain);
    }
}

result<VkShaderStageFlagBits> render::rhi::d3d12_query_shader_stage(shader shader)
{
    ZoneScoped;
    return VK_SHADER_STAGE_VERTEX_BIT;
}

result<u32> render::rhi::d3d12_query_swapchain_images_count(swapchain swapchain)
{
    ZoneScoped;
    const auto* d3d12sc = cast_from_handle<render::d3d12_swapchain>(swapchain);
    if (!d3d12sc || !d3d12sc->swapchain)
    {
        return error("Failed to access swapchain");
    }

    return d3d12sc->images.size();
}

result<u32> render::rhi::d3d12_query_current_frame_index(swapchain swapchain)
{
    ZoneScoped;
    const auto* d3d12sc = cast_from_handle<render::d3d12_swapchain>(swapchain);
    if (!d3d12sc || !d3d12sc->swapchain)
    {
        return error("Failed to access swapchain");
    }

    return d3d12sc->swapchain->GetCurrentBackBufferIndex();
}

auto render::rhi::d3d12_query_queue(context context, queue_kind kind) -> result<queue>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::d3d12_context>(context);
    if (!ctx)
    {
        return error("Failed to access context");
    }

    return queue {.id = reinterpret_cast<u64>(ctx->queues[static_cast<u32>(kind)].Get())};
}

auto render::rhi::d3d12_query_device(context context) -> result<device>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::d3d12_context>(context);
    if (!ctx)
    {
        return error("Failed to access context");
    }

    return device {.id = reinterpret_cast<u64>(ctx->device.Get())};
}

auto render::rhi::d3d12_query_physical_device(context context) -> result<physical_device>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::d3d12_context>(context);
    if (!ctx)
    {
        return error("Failed to access context");
    }

    return physical_device {.id = reinterpret_cast<u64>(ctx->adapter.Get())};
}
