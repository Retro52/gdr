#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_command_list.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_pipeline.hpp>
#include <render/platform/d3d12/d3d12_queue.hpp>
#include <render/platform/d3d12/d3d12_utils.hpp>
#include <render/platform/vk/vk_utils.hpp>
#include <render/rhi_d3d12.hpp>
#include <render/rhi_util.hpp>
#include <tracy/Tracy.hpp>

template<typename T, typename H>
static T* d3d12_rhi_object_from_handle(H handle)
{
    ZoneScoped;
    return reinterpret_cast<T*>(handle.id);
}

static D3D12_RESOURCE_STATES d3d12_rhi_prase_image_layout(const render::rhi::image_layout layout)
{
    ZoneScoped;
    switch (layout)
    {
    case render::rhi::image_layout::ePresent :
        return D3D12_RESOURCE_STATE_PRESENT;
    case render::rhi::image_layout::eRenderTargetColor :
    case render::rhi::image_layout::eRenderTargetDepthStencil :
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    default :
    case render::rhi::image_layout::eCommon :
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

static render::com_ptr<ID3D12GraphicsCommandList> d3d12_rhi_get_gfx_command_list(const render::d3d12_command_list* cmd)
{
    ZoneScoped;
    render::com_ptr<ID3D12GraphicsCommandList> gfx_command_list;
    D3D12_ASSERT_ON_FAIL(cmd->command_list.As(&gfx_command_list));

    return gfx_command_list;
}

auto render::rhi::d3d12_create_context(const window& window, const instance_desc& desc) -> result<context>
{
    ZoneScoped;
    auto d3d12_ctx = render::d3d12_create_context(window, desc);

    RESULT_FORWARD_IF_FAILED(d3d12_ctx);
    return create_handle<context>(*d3d12_ctx);
}

void render::rhi::d3d12_destroy_context(context& context)
{
    ZoneScoped;
    if (auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context))
    {
        render::d3d12_destroy_context(*d3d12_ctx);
        clear_handle(d3d12_ctx, context);
    }
}

auto render::rhi::d3d12_create_swapchain(context context, const create_swapchain_info& desc) -> result<swapchain>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    auto sc = render::d3d12_create_swapchain(
        *d3d12_ctx, d3d12_format_from_vk(desc.format), desc.size, desc.frames_in_flight, desc.vsync);

    RESULT_FORWARD_IF_FAILED(sc);
    return create_handle<swapchain>(*sc);
}

auto render::rhi::d3d12_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc)
    -> result<render::rhi::swapchain>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    auto* d3d12_sc  = cast_from_handle<render::d3d12_swapchain>(swapchain);
    if (!d3d12_ctx || !d3d12_sc)
    {
        return error("failed to access the context or the swapchain");
    }

    const bool tearing_supported = d3d12_sc->flags & swapchain_flag::eTearing;
    const UINT new_flags         = (!desc.vsync && tearing_supported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U;

    for (auto& back_buffer : d3d12_sc->back_buffers)
    {
        back_buffer.image.resource.Reset();
        d3d12_ctx->rtv_descriptor_heap.free(back_buffer.image.cpu_handle);
    }

    D3D12_RETURN_ON_FAIL(d3d12_sc->swapchain->ResizeBuffers(desc.frames_in_flight,
                                                            desc.size.x,
                                                            desc.size.y,
                                                            static_cast<DXGI_FORMAT>(d3d12_format_from_vk(desc.format)),
                                                            new_flags));

    auto new_buffers = render::d3d12_update_back_buffers(*d3d12_ctx, d3d12_sc->swapchain.Get(), desc.frames_in_flight);
    RESULT_FORWARD_IF_FAILED(new_buffers);

    d3d12_sc->flags =
        desc.vsync ? d3d12_sc->flags | swapchain_flag::eVsync : (d3d12_sc->flags & ~swapchain_flag::eVsync);
    d3d12_sc->back_buffers = std::move(*new_buffers);
    return reference_handle<render::rhi::swapchain>(d3d12_sc);
}

void render::rhi::d3d12_destroy_swapchain(context context, swapchain& swapchain)
{
    ZoneScoped;
    auto* d3d12_sc  = cast_from_handle<render::d3d12_swapchain>(swapchain);
    auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);

    if (d3d12_ctx && d3d12_sc)
    {
        render::d3d12_destroy_swapchain(*d3d12_ctx, *d3d12_sc);
        clear_handle(d3d12_sc, swapchain);
    }
}

auto render::rhi::d3d12_create_command_buffer(context context, const queue_kind queue_kind) -> result<command_buffer>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_NONE;
    switch (queue_kind)
    {
    case queue_kind::eCompute :
        type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        break;
    case queue_kind::eTransfer :
        type = D3D12_COMMAND_LIST_TYPE_COPY;
        break;
    default :
    case queue_kind::eGfx :
    case queue_kind::ePresent :
        type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        break;
    }

    auto command_list = render::d3d12_create_command_list(d3d12_ctx->device.Get(), type);
    RESULT_FORWARD_IF_FAILED(command_list);

    return create_handle<command_buffer>(*command_list);
}

void render::rhi::d3d12_destroy_command_buffer(context context, command_buffer& cmd)
{
    ZoneScoped;
    auto* d3d12_cmd       = cast_from_handle<render::d3d12_command_list>(cmd);
    const auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);

    if (d3d12_ctx && d3d12_cmd)
    {
        render::d3d12_destroy_command_list(*d3d12_cmd);
        clear_handle(d3d12_cmd, cmd);
    }
}

auto render::rhi::d3d12_create_bindless_set(context context, u32 resource_count) -> result<bindless_set>
{
    ZoneScoped;
    return null_bindless_set;
}

void render::rhi::d3d12_destroy_bindless_set(context context, bindless_set& set)
{
    ZoneScoped;
}

auto render::rhi::d3d12_create_shader(context /* context */, const fs::path& path) -> result<shader>
{
    ZoneScoped;

    const auto d3d12_shader = render::d3d12_create_shader(path);
    RESULT_FORWARD_IF_FAILED(d3d12_shader);

    return create_handle<shader>(*d3d12_shader);
}

void render::rhi::d3d12_destroy_shader(context /* context */, shader& shader)
{
    ZoneScoped;
    if (const auto d3d12_shader = cast_from_handle<render::d3d12_shader>(shader))
    {
        clear_handle(d3d12_shader, shader);
    }
}

auto render::rhi::d3d12_create_buffer(context context, const create_buffer_info& buffer_info) -> result<buffer>
{
    ZoneScoped;
    return null_buffer;
}

void render::rhi::d3d12_destroy_buffer(context context, buffer& buffer)
{
    ZoneScoped;
}

auto render::rhi::d3d12_create_compute_pso(context context, shader shader, std::span<const bindless_set> sets)
    -> result<pipeline>
{
    ZoneScoped;
    return null_pipeline;
}

auto render::rhi::d3d12_create_graphics_pso(context context, std::span<const shader> shaders,
                                            std::span<const bindless_set> sets, const pso_options& options)
    -> result<pipeline>
{
    ZoneScoped;

    const auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    d3d12_shader d3d12_shaders[16];
    assert2(shaders.size() <= COUNT_OF(d3d12_shaders));

    for (u32 i = 0; i < cpp::min(shaders.size(), COUNT_OF(d3d12_shaders)); i++)
    {
        const auto* d3d12s = cast_from_handle<d3d12_shader>(shaders[i]);
        if (!d3d12s)
        {
            return error("failed to access the shader");
        }

        d3d12_shaders[i] = *d3d12s;
    }

    const auto d3d12_pso = render::d3d12_create_pipeline_graphics(
        d3d12_ctx->device.Get(), d3d12_shaders, shaders.size(), nullptr, 0, options);
    RESULT_FORWARD_IF_FAILED(d3d12_pso);

    return create_handle<pipeline>(*d3d12_pso);
}

void render::rhi::d3d12_destroy_pso(context context, pipeline& pso)
{
    ZoneScoped;
    if (auto* d3d12_pso = cast_from_handle<render::d3d12_pipeline>(pso))
    {
        render::d3d12_destroy_pipeline(*d3d12_pso);
        clear_handle(d3d12_pso, pso);
    }
}

result<VkShaderStageFlagBits> render::rhi::d3d12_query_shader_stage(shader shader)
{
    ZoneScoped;
    if (const auto d3d12_shader = cast_from_handle<render::d3d12_shader>(shader))
    {
        return d3d12_shader->spv_meta.stage;
    }

    return error("Failed to query shader stage");
}

result<u32> render::rhi::d3d12_query_swapchain_images_count(swapchain swapchain)
{
    ZoneScoped;
    const auto* d3d12_sc = cast_from_handle<render::d3d12_swapchain>(swapchain);
    if (!d3d12_sc || !d3d12_sc->swapchain)
    {
        return error("Failed to access swapchain");
    }

    return d3d12_sc->back_buffers.size();
}

result<u32> render::rhi::d3d12_query_current_frame_index(swapchain swapchain)
{
    ZoneScoped;
    const auto* d3d12_sc = cast_from_handle<render::d3d12_swapchain>(swapchain);
    if (!d3d12_sc || !d3d12_sc->swapchain)
    {
        return error("Failed to access swapchain");
    }

    return d3d12_sc->swapchain->GetCurrentBackBufferIndex();
}

result<VkFormat> render::rhi::d3d12_query_swapchain_color_format(swapchain swapchain)
{
    if (const auto* d3d12_sc = cast_from_handle<render::d3d12_swapchain>(swapchain))
    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        D3D12_RETURN_ON_FAIL(d3d12_sc->swapchain->GetDesc1(&desc));

        return vk_format_from_dxgi(desc.Format);
    }

    return error("Failed to access swapchain");
}

auto render::rhi::d3d12_query_queue(context context, queue_kind kind) -> result<queue>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    u32 idx = render::kD3D12CommandQueueDirect;
    switch (kind)
    {
    case queue_kind::eCompute :
        idx = kD3D12CommandQueueCompute;
        break;
    case queue_kind::eTransfer :
        idx = kD3D12CommandQueueCopy;
        break;
    default :
    case queue_kind::eGfx :
    case queue_kind::ePresent :
        idx = kD3D12CommandQueueDirect;
        break;
    }

    return queue {.id = reinterpret_cast<u64>(d3d12_ctx->queues[idx].Get())};
}

auto render::rhi::d3d12_query_device(context context) -> result<device>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    return device {.id = reinterpret_cast<u64>(d3d12_ctx->device.Get())};
}

auto render::rhi::d3d12_query_physical_device(context context) -> result<physical_device>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    return physical_device {.id = reinterpret_cast<u64>(d3d12_ctx->adapter.Get())};
}

void render::rhi::d3d12_queue_wait_idle(queue queue)
{
    ZoneScoped;
    auto* d3d12_queue = d3d12_rhi_object_from_handle<ID3D12CommandQueue>(queue);
    if (!d3d12_queue)
    {
        return;
    }

    com_ptr<ID3D12Device> device;
    D3D12_ASSERT_ON_FAIL(d3d12_queue->GetDevice(IID_PPV_ARGS(&device)));

    if (auto fence = d3d12_create_fence(device.Get(), 0))
    {
        d3d12_signal_fence(*fence, d3d12_queue, 1);
        d3d12_wait_on_fence(*fence, 1);
        d3d12_destroy_fence(*fence);
    }
}

void render::rhi::d3d12_device_wait_idle(context context)
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return;
    }

    auto fence = d3d12_create_fence(d3d12_ctx->device.Get(), 0);
    if (!fence)
    {
        return;
    }

    for (u32 i = 0; i < render::kD3D12CommandQueuesCount; ++i)
    {
        d3d12_signal_fence(*fence, d3d12_ctx->queues[i].Get(), i + 1);
        d3d12_wait_on_fence(*fence, i + 1);
    }

    d3d12_destroy_fence(*fence);
}

auto render::rhi::d3d12_acquire_next_swapchain_image(context context, swapchain swapchain) -> result<image>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<render::d3d12_context>(context);
    const auto* d3d12_sc  = cast_from_handle<render::d3d12_swapchain>(swapchain);

    if (!d3d12_ctx || !d3d12_sc)
    {
        return error("Failed to access context or swapchain");
    }

    auto& back_buffer = d3d12_sc->back_buffers[d3d12_sc->swapchain->GetCurrentBackBufferIndex()];

    d3d12_wait_on_fence(d3d12_sc->sc_sync_fence, back_buffer.wait_value);
    return reference_handle<image>(&back_buffer.image);
}

void render::rhi::d3d12_cmd_begin_recording(command_buffer cmd)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<render::d3d12_command_list>(cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        D3D12_ASSERT_ON_FAIL(d3d12_cmd->allocator->Reset());
        D3D12_ASSERT_ON_FAIL(gfx_command_list->Reset(d3d12_cmd->allocator.Get(), nullptr));
    }
}

void render::rhi::d3d12_cmd_end_recording(command_buffer cmd)
{
    ZoneScoped;
    auto* d3d12_cmd = cast_from_handle<render::d3d12_command_list>(cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        D3D12_ASSERT_ON_FAIL(gfx_command_list->Close());
    }
}

void render::rhi::d3d12_cmd_transition_image(command_buffer cmd, image dst, const image_layout dst_layout)
{
    ZoneScoped;
    auto* d3d12_img       = cast_from_handle<render::d3d12_image>(dst);
    const auto* d3d12_cmd = cast_from_handle<render::d3d12_command_list>(cmd);
    if (!d3d12_cmd || !d3d12_img)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        const D3D12_RESOURCE_BARRIER barrier {
            .Type       = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags      = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {.pResource   = d3d12_img->resource.Get(),
                           .StateBefore = d3d12_img->current_state,
                           .StateAfter  = d3d12_rhi_prase_image_layout(dst_layout)}
        };

        d3d12_img->current_state = barrier.Transition.StateAfter;
        gfx_command_list->ResourceBarrier(1, &barrier);
    }
}

void render::rhi::d3d12_cmd_present_image(command_buffer cmd, swapchain swapchain, queue submit, queue present)
{
    ZoneScoped;
    auto* d3d12_sc        = cast_from_handle<render::d3d12_swapchain>(swapchain);
    auto* d3d12_queue     = d3d12_rhi_object_from_handle<ID3D12CommandQueue>(submit);
    const auto* d3d12_cmd = cast_from_handle<render::d3d12_command_list>(cmd);

    assert2(d3d12_queue == d3d12_rhi_object_from_handle<ID3D12CommandQueue>(present));
    if (!d3d12_cmd || !d3d12_sc)
    {
        return;
    }

    ID3D12CommandList* const command_lists[] = {d3d12_cmd->command_list.Get()};
    d3d12_queue->ExecuteCommandLists(COUNT_OF(command_lists), command_lists);

    const auto bb_index      = d3d12_sc->swapchain->GetCurrentBackBufferIndex();
    const auto sync_interval = d3d12_sc->flags & swapchain_flag::eVsync ? 1 : 0;
    const auto present_flags =
        d3d12_sc->flags & swapchain_flag::eTearing && !sync_interval ? DXGI_PRESENT_ALLOW_TEARING : 0;

    D3D12_ASSERT_ON_FAIL(d3d12_sc->swapchain->Present(sync_interval, present_flags));
    d3d12_signal_fence(d3d12_sc->sc_sync_fence, d3d12_queue, ++d3d12_sc->frame_counter);
    d3d12_sc->back_buffers[bb_index].wait_value = d3d12_sc->frame_counter;
}

void render::rhi::d3d12_cmd_set_draw_state(command_buffer cmd, std::span<const attachment_state_info> color_attachments,
                                           attachment_state_info depth_attachment, uvec4 viewport)
{
    ZoneScoped;
    auto* d3d12_cmd = cast_from_handle<render::d3d12_command_list>(cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd);
    if (!gfx_command_list)
    {
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE color_handles[16] {};
    assert2(color_attachments.size() <= COUNT_OF(color_handles));

    for (u32 i = 0; i < cpp::min(color_attachments.size(), COUNT_OF(color_handles)); i++)
    {
        const auto* d3d12_img = cast_from_handle<render::d3d12_image>(color_attachments[i].attachment);
        if (!d3d12_img)
        {
            return;
        }

        color_handles[i] = d3d12_img->cpu_handle;

        switch (color_attachments[i].load_op)
        {
        case resource_load_op::eDiscard :
            gfx_command_list->DiscardResource(d3d12_img->resource.Get(), nullptr);
            break;
        case resource_load_op::eClear :
            gfx_command_list->ClearRenderTargetView(
                d3d12_img->cpu_handle, &color_attachments[i].clear_value.f4.x, 0, nullptr);
            break;
        default :
        case resource_load_op::eLoad :
            break;
        }
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE* depth_stencil = nullptr;
    if (depth_attachment)
    {
        const auto* d3d12_img = cast_from_handle<render::d3d12_image>(depth_attachment.attachment);
        if (!d3d12_img)
        {
            return;
        }

        depth_stencil = &d3d12_img->cpu_handle;

        switch (depth_attachment.load_op)
        {
        case resource_load_op::eDiscard :
            gfx_command_list->DiscardResource(d3d12_img->resource.Get(), nullptr);
            break;
        case resource_load_op::eClear :
            gfx_command_list->ClearDepthStencilView(d3d12_img->cpu_handle,
                                                    D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                                    depth_attachment.clear_value.ds.depth,
                                                    depth_attachment.clear_value.ds.stencil,
                                                    0,
                                                    nullptr);
            break;
        default :
        case resource_load_op::eLoad :
            break;
        }
    }

    {

    };

    D3D12_RECT scissor = CD3DX12_RECT(static_cast<LONG>(viewport.x),
                                      static_cast<LONG>(viewport.y),
                                      static_cast<LONG>(viewport.z),
                                      static_cast<LONG>(viewport.w));

    D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(static_cast<FLOAT>(viewport.x),
                                         static_cast<FLOAT>(viewport.y + viewport.w),
                                         static_cast<FLOAT>(viewport.z),
                                         -static_cast<FLOAT>(viewport.w));

    gfx_command_list->RSSetViewports(1, &vp);
    gfx_command_list->RSSetScissorRects(1, &scissor);
    gfx_command_list->OMSetRenderTargets(color_attachments.size(), color_handles, FALSE, depth_stencil);
}

void render::rhi::d3d12_cmd_clear_draw_state(command_buffer cmd)
{
    ZoneScoped;
}

void render::rhi::d3d12_cmd_bind_pso(command_buffer cmd, pipeline pso)
{
    ZoneScoped;

    const auto* d3d12_pso = cast_from_handle<render::d3d12_pipeline>(pso);
    const auto* d3d12_cmd = cast_from_handle<render::d3d12_command_list>(cmd);
    if (!d3d12_cmd || !d3d12_pso)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->SetPipelineState(d3d12_pso->pso.Get());
        gfx_command_list->SetGraphicsRootSignature(d3d12_pso->root_signature.Get());

        gfx_command_list->IASetPrimitiveTopology(d3d12_pso->topology);
    }
}

void render::rhi::d3d12_cmd_draw_instanced(command_buffer cmd, const u32 vtx_count, const u32 instance_count,
                                           const u32 first_vertex, const u32 first_instance)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<render::d3d12_command_list>(cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->DrawInstanced(vtx_count, instance_count, first_vertex, first_instance);
    }
}
