#if GDR_ENABLE_DX12_BACKEND

#include <cpp/alg_constexpr.hpp>
#include <cpp/containers/local_array.hpp>
#include <ddspp.h>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_buffer.hpp>
#include <render/platform/d3d12/d3d12_command_list.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_ffx.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>
#include <render/platform/d3d12/d3d12_pipeline.hpp>
#include <render/platform/d3d12/d3d12_sampler.hpp>
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

static u32 d3d12_align_wg(const u32 global, const u32 local)
{
    return (global + local - 1) / local;
}

static uvec3 d3d12_align_wg3(const uvec3 global, const uvec3 local)
{
    return {
        d3d12_align_wg(global.x, local.x),
        d3d12_align_wg(global.y, local.y),
        d3d12_align_wg(global.z, local.z),
    };
}

static void d3d12_rhi_set_object_name_if_needed(const platform::d3d12_context& ctx, ID3D12Object* object,
                                                const char* name)
{
    ZoneScoped;
    if (!name || !object || !ctx.enabled_device_features.enabled(rhi::feature_flag::debug_labels))
    {
        return;
    }

    const auto wc_count = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (wc_count <= 0)
    {
        return;
    }

    std::wstring w_str(wc_count - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, name, -1, w_str.data(), wc_count);

    D3D12_ASSERT_ON_FAIL(object->SetName(w_str.c_str()));
}

static platform::d3d12_heaps_view d3d12_rhi_heaps_view(platform::d3d12_context& ctx)
{
    return platform::d3d12_heaps_view {
        .rtv_heap         = ctx.rtv_descriptor_heap,
        .dsv_heap         = ctx.dsv_descriptor_heap,
        .cbv_srv_uav_heap = ctx.cbv_srv_uav_descriptor_heap,
    };
}

static uvec2 d3d12_rhi_subresource_counts(const D3D12_RESOURCE_DESC& desc)
{
    const u32 layers = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : desc.DepthOrArraySize;
    return {desc.MipLevels, layers};
}

static const platform::d3d12_image_view* d3d12_rhi_view_of(const rhi::attachment attachment)
{
    if (attachment.is_image_view())
    {
        return rhi::cast_from_handle<platform::d3d12_image_view>(attachment.get_image_view());
    }

    const auto* d3d12_image = rhi::cast_from_handle<platform::d3d12_image>(attachment.get_image());
    return d3d12_image ? &d3d12_image->view : nullptr;
}

static rhi::image_view_kind d3d12_rhi_default_view_kind(const rhi::create_image_info& image_info)
{
    if (image_info.dimensions.y <= 1)
    {
        return image_info.layer_count > 1 ? rhi::image_view_kind::array_1d : rhi::image_view_kind::flat_1d;
    }

    if (image_info.dimensions.z > 1)
    {
        return rhi::image_view_kind::flat_3d;
    }

    return image_info.layer_count > 1 ? rhi::image_view_kind::array_2d : rhi::image_view_kind::flat_2d;
}

static D3D12_GLOBAL_BARRIER d3d12_rhi_parse_global_barrier(const rhi::global_barrier global_barrier)
{
    return D3D12_GLOBAL_BARRIER {
        .SyncBefore   = platform::d3d12_rhi_parse_stage_flags(global_barrier.before.stages),
        .SyncAfter    = platform::d3d12_rhi_parse_stage_flags(global_barrier.after.stages),
        .AccessBefore = platform::d3d12_rhi_parse_access_flags(global_barrier.before.access),
        .AccessAfter  = platform::d3d12_rhi_parse_access_flags(global_barrier.after.access),
    };
}

static D3D12_TEXTURE_BARRIER d3d12_rhi_parse_image_barrier(const rhi::image_barrier& image_barrier)
{
    auto* d3d12_image = rhi::cast_from_handle<platform::d3d12_image>(image_barrier.image);
    assert2(d3d12_image);
    if (!d3d12_image)
    {
        return {};
    }

    const auto old_layout = image_barrier.layout_before == rhi::image_layout::current
                              ? d3d12_image->layout
                              : platform::d3d12_rhi_parse_image_layout(image_barrier.layout_before);

    const auto new_layout = image_barrier.layout_after == rhi::image_layout::current
                              ? d3d12_image->layout
                              : platform::d3d12_rhi_parse_image_layout(image_barrier.layout_after);

    const auto counts   = d3d12_rhi_subresource_counts(d3d12_image->resource->GetDesc());
    d3d12_image->layout = new_layout;
    return D3D12_TEXTURE_BARRIER {
        .SyncBefore   = platform::d3d12_rhi_parse_stage_flags(image_barrier.before.stages),
        .SyncAfter    = platform::d3d12_rhi_parse_stage_flags(image_barrier.after.stages),
        .AccessBefore = platform::d3d12_rhi_parse_access_flags(image_barrier.before.access),
        .AccessAfter  = platform::d3d12_rhi_parse_access_flags(image_barrier.after.access),
        .LayoutBefore = old_layout,
        .LayoutAfter  = new_layout,
        .pResource    = d3d12_image->resource.Get(),
        .Subresources = platform::d3d12_rhi_parse_image_range(image_barrier.range, counts.x, counts.y),
        .Flags        = image_barrier.layout_after == rhi::image_layout::discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD
                                                                                 : D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

static std::pair<D3D12_BARRIER_SYNC, D3D12_BARRIER_ACCESS> d3d12_rhi_buffer_barrier_scope(
    const rhi::barrier_scope& scope)
{
    auto sync   = platform::d3d12_rhi_parse_stage_flags(scope.stages);
    auto access = platform::d3d12_rhi_parse_access_flags(scope.access);

    if (scope.access & rhi::barrier_access::copy_write)
    {
        sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
        access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    }

    return {sync, access};
}

static D3D12_BUFFER_BARRIER d3d12_rhi_parse_buffer_barrier(const rhi::buffer_barrier& buffer_barrier)
{
    const auto* d3d12_buffer = rhi::cast_from_handle<platform::d3d12_buffer>(buffer_barrier.buffer);
    assert2(d3d12_buffer);
    if (!d3d12_buffer)
    {
        return {};
    }

    const auto [before_sync, before_access] = d3d12_rhi_buffer_barrier_scope(buffer_barrier.before);
    const auto [after_sync, after_access]   = d3d12_rhi_buffer_barrier_scope(buffer_barrier.after);

    return D3D12_BUFFER_BARRIER {
        .SyncBefore   = before_sync,
        .SyncAfter    = after_sync,
        .AccessBefore = before_access,
        .AccessAfter  = after_access,
        .pResource    = d3d12_buffer->allocation->GetResource(),
        .Offset       = buffer_barrier.data_range.x,
        .Size         = buffer_barrier.data_range.y == rhi::kBufferAll ? UINT64_MAX : buffer_barrier.data_range.y,
    };
}

template<typename T = ID3D12GraphicsCommandList>
static platform::com_ptr<T> d3d12_rhi_get_gfx_command_list(const platform::d3d12_command_list* cmd)
{
    ZoneScoped;
    platform::com_ptr<T> gfx_command_list;
    D3D12_ASSERT_ON_FAIL(cmd->command_list.As(&gfx_command_list));

    return gfx_command_list;
}

static void d3d12_rhi_refresh_pso_bind(const platform::d3d12_command_list* cmd)
{
    if (!cmd || !cmd->pso)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list<ID3D12GraphicsCommandList9>(cmd))
    {
        cmd->pso->bind(gfx_command_list.Get(), cmd->cull_mode, cmd->depth_bias_params);
    }
}

auto rhi::d3d12_create_context(const window& window, const instance_desc& desc) -> result<context>
{
    ZoneScoped;
    auto d3d12_ctx = platform::d3d12_create_context(window, desc);

    RESULT_FORWARD_IF_FAILED(d3d12_ctx);
    return create_handle<context>(*d3d12_ctx);
}

void rhi::d3d12_destroy_context(context& context)
{
    ZoneScoped;
    if (auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context))
    {
        platform::d3d12_destroy_context(*d3d12_ctx);
        clear_handle(d3d12_ctx, context);
    }
}

auto rhi::d3d12_create_swapchain(context context, const create_swapchain_info& desc) -> result<swapchain>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    auto sc = platform::d3d12_create_swapchain(*d3d12_ctx,
                                               platform::d3d12_format_from_vk(static_cast<VkFormat>(desc.format)),
                                               desc.size,
                                               desc.frames_in_flight,
                                               desc.vsync);

    RESULT_FORWARD_IF_FAILED(sc);
    return create_handle<swapchain>(*sc);
}

auto rhi::d3d12_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc)
    -> result<rhi::swapchain>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_sc  = cast_from_handle<platform::d3d12_swapchain>(swapchain);
    if (!d3d12_ctx || !d3d12_sc)
    {
        return error("failed to access the context or the swapchain");
    }

    const bool tearing_supported = d3d12_sc->flags & platform::swapchain_flag::tearing;
    const UINT new_flags         = (!desc.vsync && tearing_supported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U;

    for (auto& back_buffer : d3d12_sc->back_buffers)
    {
        back_buffer.image.resource.Reset();
        d3d12_ctx->rtv_descriptor_heap.free(back_buffer.image.view.descriptors.rtv);
    }

    D3D12_RETURN_ON_FAIL(
        d3d12_sc->swapchain->ResizeBuffers(desc.frames_in_flight,
                                           desc.size.x,
                                           desc.size.y,
                                           platform::d3d12_format_from_vk(static_cast<VkFormat>(desc.format)),
                                           new_flags));

    auto new_buffers =
        platform::d3d12_update_back_buffers(*d3d12_ctx, d3d12_sc->swapchain.Get(), desc.frames_in_flight);
    RESULT_FORWARD_IF_FAILED(new_buffers);

    d3d12_sc->flags        = desc.vsync ? d3d12_sc->flags | platform::swapchain_flag::vsync
                                        : (d3d12_sc->flags & ~platform::swapchain_flag::vsync);
    d3d12_sc->back_buffers = std::move(*new_buffers);
    return reference_handle<rhi::swapchain>(d3d12_sc);
}

void rhi::d3d12_destroy_swapchain(context context, swapchain& swapchain)
{
    ZoneScoped;
    auto* d3d12_sc  = cast_from_handle<platform::d3d12_swapchain>(swapchain);
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);

    if (d3d12_ctx && d3d12_sc)
    {
        platform::d3d12_destroy_swapchain(*d3d12_ctx, *d3d12_sc);
        clear_handle(d3d12_sc, swapchain);
    }
}

auto rhi::d3d12_create_command_buffer(context context, const queue_kind queue_kind) -> result<command_buffer>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_NONE;
    switch (queue_kind)
    {
    case queue_kind::compute :
        type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        break;
    case queue_kind::transfer :
        type = D3D12_COMMAND_LIST_TYPE_COPY;
        break;
    default :
    case queue_kind::gfx :
    case queue_kind::present :
        type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        break;
    }

    const std::array shader_visible_heaps = {d3d12_ctx->sampler_descriptor_heap.heap.Get(),
                                             d3d12_ctx->cbv_srv_uav_descriptor_heap.heap.Get()};

    auto command_list = platform::d3d12_create_command_list(
        d3d12_ctx->device.Get(), d3d12_ctx->allocator.Get(), type, shader_visible_heaps);
    RESULT_FORWARD_IF_FAILED(command_list);

    return create_handle<command_buffer>(*command_list);
}

void rhi::d3d12_destroy_command_buffer(context context, command_buffer& cmd)
{
    ZoneScoped;
    auto* d3d12_cmd       = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);

    if (d3d12_ctx && d3d12_cmd)
    {
        platform::d3d12_destroy_command_list(*d3d12_cmd);
        clear_handle(d3d12_cmd, cmd);
    }
}

auto rhi::d3d12_create_bindless_set(context context, const u32 resource_count) -> result<bindless_set>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("failed to access context");
    }

    platform::d3d12_view_desc_range d3d12_set;
    d3d12_set.count  = resource_count;
    d3d12_set.stride = d3d12_ctx->cbv_srv_uav_descriptor_heap.stride;
    d3d12_set.start  = d3d12_ctx->cbv_srv_uav_descriptor_heap.alloc(resource_count);

    return create_handle<bindless_set>(d3d12_set);
}

void rhi::d3d12_update_bindless_set(context context, bindless_set set,
                                    const std::span<const bindless_set_write_info> info)
{
    ZoneScoped;
    auto* d3d12_ctx       = cast_from_handle<platform::d3d12_context>(context);
    const auto* d3d12_set = cast_from_handle<platform::d3d12_view_desc_range>(set);

    assert2(d3d12_ctx && d3d12_set);
    if (!d3d12_ctx || !d3d12_set)
    {
        return;
    }

    for (const auto& write : info)
    {
        const auto* d3d12_view = d3d12_rhi_view_of(write.dst);

        assert2(d3d12_view);
        assert2m(write.index < d3d12_set->count, "bindless write index is out of the set's range");

        if (!d3d12_view || write.index >= d3d12_set->count) [[unlikely]]
        {
            continue;
        }

        platform::d3d12_write_srv(d3d12_ctx->device.Get(), *d3d12_view, d3d12_set->cpu_at(write.index));
    }
}

void rhi::d3d12_destroy_bindless_set(context context, bindless_set& set)
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_set = cast_from_handle<platform::d3d12_view_desc_range>(set);
    if (d3d12_ctx && d3d12_set)
    {
        d3d12_ctx->cbv_srv_uav_descriptor_heap.free(d3d12_set->start, d3d12_set->count);
        clear_handle(d3d12_set, set);
    }
}

auto rhi::d3d12_create_fence(context context, const u64 initial_value) -> result<fence>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("failed to access context");
    }

    const auto d3d12_fence = platform::d3d12_create_fence(d3d12_ctx->device.Get(), initial_value);
    RESULT_FORWARD_IF_FAILED(d3d12_fence);

    return create_handle<fence>(*d3d12_fence);
}

void rhi::d3d12_destroy_fence(context context, fence& fence)
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_fence     = cast_from_handle<platform::d3d12_fence>(fence);

    if (d3d12_ctx && d3d12_fence)
    {
        platform::d3d12_destroy_fence(*d3d12_fence);
        clear_handle(d3d12_fence, fence);
    }
}

auto rhi::d3d12_create_shader(context /* context */, const fs::path& path) -> result<shader>
{
    ZoneScoped;

    const auto d3d12_shader = platform::d3d12_create_shader(path);
    RESULT_FORWARD_IF_FAILED(d3d12_shader);

    return create_handle<shader>(*d3d12_shader);
}

void rhi::d3d12_destroy_shader(context /* context */, shader& shader)
{
    ZoneScoped;
    if (const auto d3d12_shader = cast_from_handle<platform::d3d12_shader>(shader))
    {
        platform::d3d12_destroy_shader(*d3d12_shader);
        clear_handle(d3d12_shader, shader);
    }
}

auto rhi::d3d12_create_buffer(context context, const create_buffer_info& buffer_info) -> result<buffer>
{
    ZoneScoped;
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("failed to access context");
    }

    const auto flags = (buffer_info.usage_flags & rhi::buffer_usage::shader_rw && !buffer_info.mapped)
                         ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                         : D3D12_RESOURCE_FLAG_NONE;

    auto d3d12_buffer =
        platform::d3d12_create_buffer(d3d12_ctx->allocator.Get(),
                                      buffer_info.size,
                                      buffer_info.mapped ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT,
                                      flags);

    RESULT_FORWARD_IF_FAILED(d3d12_buffer);

    if (buffer_info.mapped)
    {
        assert2(d3d12_buffer->mapped);
        *buffer_info.mapped = d3d12_buffer->mapped;
    }

    if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        platform::d3d12_create_buffer_uav(d3d12_ctx->device.Get(),
                                          *d3d12_buffer,
                                          d3d12_ctx->cbv_srv_uav_descriptor_heap,
                                          d3d12_ctx->cbv_srv_uav_cpu_descriptor_heap);
    }

    d3d12_rhi_set_object_name_if_needed(*d3d12_ctx, d3d12_buffer->allocation->GetResource(), buffer_info.dbg_name);
    return create_handle<buffer>(*d3d12_buffer);
}

void rhi::d3d12_destroy_buffer(context context, buffer& buffer)
{
    ZoneScoped;
    auto* d3d12_ctx    = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_buffer = cast_from_handle<platform::d3d12_buffer>(buffer);

    if (d3d12_ctx && d3d12_buffer)
    {
        platform::d3d12_destroy_buffer_uav(
            *d3d12_buffer, d3d12_ctx->cbv_srv_uav_descriptor_heap, d3d12_ctx->cbv_srv_uav_cpu_descriptor_heap);
        platform::d3d12_destroy_buffer(*d3d12_buffer);
        clear_handle(d3d12_buffer, buffer);
    }
}

auto rhi::d3d12_create_image(context context, const create_image_info& image_info) -> result<image>
{
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("failed to access context");
    }

    const auto d3d12_type =
        image_info.dimensions.y > 1
            ? (image_info.dimensions.z > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            : D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    const auto d3d12_dimensions = uvec3(
        image_info.dimensions.x, image_info.dimensions.y, cpp::max(image_info.dimensions.z, image_info.layer_count));

    auto d3d12_image =
        platform::d3d12_create_image(d3d12_ctx->allocator.Get(),
                                     d3d12_type,
                                     platform::d3d12_format_from_vk(static_cast<VkFormat>(image_info.format)),
                                     image_info.mips_count,
                                     d3d12_dimensions,
                                     platform::d3d12_rhi_parse_image_usage(image_info.usage_flags));
    RESULT_FORWARD_IF_FAILED(d3d12_image);

    const auto resource_desc = d3d12_image->resource->GetDesc();
    const auto counts        = d3d12_rhi_subresource_counts(resource_desc);

    const platform::d3d12_image_view_desc meta {
        .mips   = uvec2(0, static_cast<u16>(counts.x)),
        .layers = uvec2(0, static_cast<u16>(counts.y)),
        .format = platform::d3d12_format_from_vk(static_cast<VkFormat>(image_info.format)),
        .kind   = d3d12_rhi_default_view_kind(image_info),
    };

    auto d3d12_view =
        platform::d3d12_create_image_view(d3d12_ctx->device.Get(),
                                          d3d12_image->resource.Get(),
                                          d3d12_rhi_heaps_view(*d3d12_ctx),
                                          meta,
                                          platform::d3d12_view_types_from_resource_flags(resource_desc.Flags));

    if (!d3d12_view)
    {
        platform::d3d12_destroy_image(*d3d12_image);
        return error(d3d12_view.message);
    }

    d3d12_image->view = *d3d12_view;

    d3d12_rhi_set_object_name_if_needed(*d3d12_ctx, d3d12_image->resource.Get(), image_info.dbg_name);
    return create_handle<image>(*d3d12_image);
}

void rhi::d3d12_destroy_image(context context, image& image)
{
    auto* d3d12_ctx   = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_image = cast_from_handle<platform::d3d12_image>(image);
    if (d3d12_ctx && d3d12_image)
    {
        platform::d3d12_destroy_image_view(d3d12_image->view, d3d12_rhi_heaps_view(*d3d12_ctx));
        platform::d3d12_destroy_image(*d3d12_image);
        clear_handle(d3d12_image, image);
    }
}

auto rhi::d3d12_create_image_view(context context, image source, const create_image_view_info& view_info)
    -> result<image_view>
{
    auto* d3d12_ctx         = cast_from_handle<platform::d3d12_context>(context);
    const auto* d3d12_image = cast_from_handle<platform::d3d12_image>(source);

    if (!d3d12_ctx || !d3d12_image)
    {
        return error("failed to access context or image source");
    }

    const auto resource_desc = d3d12_image->resource->GetDesc();
    const auto counts        = d3d12_rhi_subresource_counts(resource_desc);
    const auto& range        = view_info.range;

    const platform::d3d12_image_view_desc meta {
        .mips   = uvec2(range.mips_range.x,
                      range.mips_range.y == kMipsAll ? counts.x - range.mips_range.x : range.mips_range.y),
        .layers = uvec2(range.layers_range.x,
                        range.layers_range.y == kLayersAll ? counts.y - range.layers_range.x : range.layers_range.y),
        .format = platform::d3d12_format_from_vk(static_cast<VkFormat>(view_info.format)),
        .kind   = view_info.kind,
    };

    const auto d3d12_view =
        platform::d3d12_create_image_view(d3d12_ctx->device.Get(),
                                          d3d12_image->resource.Get(),
                                          d3d12_rhi_heaps_view(*d3d12_ctx),
                                          meta,
                                          platform::d3d12_view_types_from_resource_flags(resource_desc.Flags));
    RESULT_FORWARD_IF_FAILED(d3d12_view);

    return create_handle<image_view>(*d3d12_view);
}

void rhi::d3d12_destroy_image_view(context context, image_view& view)
{
    auto* d3d12_ctx  = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_view = cast_from_handle<platform::d3d12_image_view>(view);

    if (d3d12_ctx && d3d12_view)
    {
        platform::d3d12_destroy_image_view(*d3d12_view, d3d12_rhi_heaps_view(*d3d12_ctx));
        clear_handle(d3d12_view, view);
    }
}

auto rhi::d3d12_create_sampler(context context, const create_sampler_info& sampler_info) -> result<sampler>
{
    auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("failed to access context");
    }

    vec4 border_color;
    switch (sampler_info.border_color)
    {
    default :
    case rhi::sampler_border_color::white :
        border_color = vec4(1.0F);
        break;
    case rhi::sampler_border_color::black :
        border_color = vec4(vec3(0.0F), 1.0F);
        break;
    case rhi::sampler_border_color::black_transparent :
        border_color = vec4(0.0F);
        break;
    }

    return create_handle<sampler>(platform::d3d12_create_sampler(
        d3d12_ctx->device.Get(),
        d3d12_ctx->sampler_descriptor_heap,
        platform::d3d12_rhi_parse_filter_type(sampler_info.filter, sampler_info.reduction, sampler_info.mipmap_mode),
        platform::d3d12_rhi_parse_address_mode(sampler_info.address_mode),
        sampler_info.anisotropy_factor,
        platform::d3d12_rhi_parse_comparison_func(sampler_info.compare_op),
        border_color));
}

void rhi::d3d12_destroy_sampler(context context, sampler& sampler)
{
    auto* d3d12_ctx     = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_sampler = cast_from_handle<platform::d3d12_sampler>(sampler);
    if (d3d12_ctx && d3d12_sampler)
    {
        platform::d3d12_destroy_sampler(d3d12_ctx->sampler_descriptor_heap, *d3d12_sampler);
        clear_handle(d3d12_sampler, sampler);
    }
}

auto rhi::d3d12_create_compute_pso(context context, shader shader, std::span<const bindless_set> /* sets */)
    -> result<pipeline>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    const auto* d3d12s = cast_from_handle<platform::d3d12_shader>(shader);
    if (!d3d12s)
    {
        return error("failed to access the shader");
    }

    const auto d3d12_pso = platform::d3d12_create_pipeline_compute(d3d12_ctx->device.Get(), *d3d12s);
    RESULT_FORWARD_IF_FAILED(d3d12_pso);

    return create_handle<pipeline>(*d3d12_pso);
}

auto rhi::d3d12_create_graphics_pso(context context, const std::span<const shader> shaders,
                                    std::span<const bindless_set> /* sets */, const pso_options& options)
    -> result<pipeline>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    platform::d3d12_shader d3d12_shaders[16];
    assert2(shaders.size() <= COUNT_OF(d3d12_shaders));

    for (u32 i = 0; i < cpp::min(shaders.size(), COUNT_OF(d3d12_shaders)); i++)
    {
        const auto* d3d12s = cast_from_handle<platform::d3d12_shader>(shaders[i]);
        if (!d3d12s)
        {
            return error("failed to access the shader");
        }

        d3d12_shaders[i] = *d3d12s;
    }

    const auto d3d12_pso =
        platform::d3d12_create_pipeline_graphics(d3d12_ctx->device.Get(), d3d12_shaders, shaders.size(), options);
    RESULT_FORWARD_IF_FAILED(d3d12_pso);

    return create_handle<pipeline>(*d3d12_pso);
}

void rhi::d3d12_destroy_pso(context /* context */, pipeline& pso)
{
    ZoneScoped;
    if (auto* d3d12_pso = cast_from_handle<platform::d3d12_pipeline>(pso))
    {
        platform::d3d12_destroy_pipeline(*d3d12_pso);
        clear_handle(d3d12_pso, pso);
    }
}

auto rhi::d3d12_query_shader_stage(shader shader) -> result<shader_stage>
{
    ZoneScoped;
    if (const auto d3d12_shader = cast_from_handle<platform::d3d12_shader>(shader))
    {
        return platform::d3d12_rhi_translate_shader_stage(d3d12_shader->dxil_meta.shader_type);
    }

    return error("Failed to query shader stage");
}

result<u32> rhi::d3d12_query_current_frame_index(swapchain swapchain)
{
    ZoneScoped;
    const auto* d3d12_sc = cast_from_handle<platform::d3d12_swapchain>(swapchain);
    if (!d3d12_sc || !d3d12_sc->swapchain)
    {
        return error("Failed to access swapchain");
    }

    return d3d12_sc->swapchain->GetCurrentBackBufferIndex();
}

result<u32> rhi::d3d12_query_swapchain_images_count(swapchain swapchain)
{
    ZoneScoped;
    const auto* d3d12_sc = cast_from_handle<platform::d3d12_swapchain>(swapchain);
    if (!d3d12_sc || !d3d12_sc->swapchain)
    {
        return error("Failed to access swapchain");
    }

    return d3d12_sc->back_buffers.size();
}

auto rhi::d3d12_query_swapchain_color_format(swapchain swapchain) -> result<image_format>
{
    if (const auto* d3d12_sc = cast_from_handle<platform::d3d12_swapchain>(swapchain))
    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        D3D12_RETURN_ON_FAIL(d3d12_sc->swapchain->GetDesc1(&desc));

        return static_cast<image_format>(platform::vk_format_from_dxgi(desc.Format));
    }

    return error("Failed to access swapchain");
}

auto rhi::d3d12_query_feature_support(context context, const feature_flag feature) -> result<bool>
{
    ZoneScoped;
    if (const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context))
    {
        return d3d12_ctx->enabled_device_features.supported(feature);
    }

    return error("failed to access the context");
}

auto rhi::d3d12_query_queue(context context, const queue_kind kind) -> result<queue>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    u32 idx = platform::kD3D12_CommandQueueDirect;
    switch (kind)
    {
    case queue_kind::compute :
        idx = platform::kD3D12_CommandQueueCompute;
        break;
    case queue_kind::transfer :
        idx = platform::kD3D12_CommandQueueCopy;
        break;
    default :
    case queue_kind::gfx :
    case queue_kind::present :
        idx = platform::kD3D12_CommandQueueDirect;
        break;
    }

    return queue {.id = reinterpret_cast<u64>(d3d12_ctx->queues[idx].Get())};
}

auto rhi::d3d12_query_device(context context) -> result<device>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    return device {.id = reinterpret_cast<u64>(d3d12_ctx->device.Get())};
}

auto rhi::d3d12_query_physical_device(context context) -> result<physical_device>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return error("Failed to access context");
    }

    return physical_device {.id = reinterpret_cast<u64>(d3d12_ctx->adapter.Get())};
}

void rhi::d3d12_queue_wait_idle(const queue queue)
{
    ZoneScoped;
    auto* d3d12_queue = d3d12_rhi_object_from_handle<ID3D12CommandQueue>(queue);
    if (!d3d12_queue)
    {
        return;
    }

    platform::com_ptr<ID3D12Device> device;
    D3D12_ASSERT_ON_FAIL(d3d12_queue->GetDevice(IID_PPV_ARGS(&device)));

    if (auto fence = platform::d3d12_create_fence(device.Get(), 0))
    {
        platform::d3d12_signal_fence(*fence, d3d12_queue, 1);
        platform::d3d12_wait_on_fence(*fence, 1);
        platform::d3d12_destroy_fence(*fence);
    }
}

void rhi::d3d12_device_wait_idle(context context)
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    if (!d3d12_ctx)
    {
        return;
    }

    auto fence = platform::d3d12_create_fence(d3d12_ctx->device.Get(), 0);
    if (!fence)
    {
        return;
    }

    for (u32 i = 0; i < platform::kD3D12_CommandQueuesCount; ++i)
    {
        platform::d3d12_signal_fence(*fence, d3d12_ctx->queues[i].Get(), i + 1);
        platform::d3d12_wait_on_fence(*fence, i + 1);
    }

    platform::d3d12_destroy_fence(*fence);
}

void rhi::d3d12_fence_wait_for_value(context /* context */, fence fence, const u64 expected_value)
{
    const auto d3d12_fence = cast_from_handle<platform::d3d12_fence>(fence);
    if (!d3d12_fence)
    {
        return;
    }

    platform::d3d12_wait_on_fence(*d3d12_fence, expected_value);
}

auto rhi::d3d12_acquire_next_swapchain_image(context context, swapchain swapchain) -> result<image>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(context);
    const auto* d3d12_sc  = cast_from_handle<platform::d3d12_swapchain>(swapchain);

    if (!d3d12_ctx || !d3d12_sc)
    {
        return error("Failed to access context or swapchain");
    }

    auto& back_buffer = d3d12_sc->back_buffers[d3d12_sc->swapchain->GetCurrentBackBufferIndex()];

    d3d12_wait_on_fence(d3d12_sc->sc_sync_fence, back_buffer.wait_value);
    return reference_handle<image>(&back_buffer.image);
}

void rhi::d3d12_cmd_begin_recording(command_buffer cmd)
{
    ZoneScoped;
    auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        d3d12_cmd->scratch_head = 0;
        D3D12_ASSERT_ON_FAIL(d3d12_cmd->allocator->Reset());
        D3D12_ASSERT_ON_FAIL(gfx_command_list->Reset(d3d12_cmd->allocator.Get(), nullptr));

        if (d3d12_cmd->shader_visible_heaps[0])
        {
            gfx_command_list->SetDescriptorHeaps(platform::kD3D12_ShaderVisibleHeapsCount,
                                                 d3d12_cmd->shader_visible_heaps.data());
        }
    }
}

void rhi::d3d12_cmd_end_recording(command_buffer cmd)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        D3D12_ASSERT_ON_FAIL(gfx_command_list->Close());
    }
}

void rhi::d3d12_cmd_reset(command_buffer /* cmd */)
{
    // d3d12_cmd_begin_recording does all that
}

void rhi::d3d12_cmd_barriers(command_buffer cmd, const barrier_batch& barriers)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);

    assert2(d3d12_cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    const auto gfx_command_list = d3d12_rhi_get_gfx_command_list<ID3D12GraphicsCommandList7>(d3d12_cmd);
    assert2(gfx_command_list);
    if (!gfx_command_list)
    {
        return;
    }

    cpp::local_array<D3D12_GLOBAL_BARRIER, 8> global_barriers(barriers.globals.size());
    cpp::local_array<D3D12_TEXTURE_BARRIER, 8> image_barriers(barriers.images.size());
    cpp::local_array<D3D12_BUFFER_BARRIER, 8> buffer_barriers(barriers.buffers.size());

    for (u32 i = 0; i < barriers.globals.size(); ++i)
    {
        global_barriers[i] = d3d12_rhi_parse_global_barrier(barriers.globals[i]);
    }

    for (u32 i = 0; i < barriers.images.size(); ++i)
    {
        image_barriers[i] = d3d12_rhi_parse_image_barrier(barriers.images[i]);
    }

    for (u32 i = 0; i < barriers.buffers.size(); ++i)
    {
        buffer_barriers[i] = d3d12_rhi_parse_buffer_barrier(barriers.buffers[i]);
    }

    cpp::local_array<D3D12_BARRIER_GROUP, 3> d3d12_barrier_groups;
    if (!global_barriers.empty())
    {
        d3d12_barrier_groups.emplace_back(D3D12_BARRIER_GROUP {
            .Type            = D3D12_BARRIER_TYPE_GLOBAL,
            .NumBarriers     = static_cast<u32>(global_barriers.size()),
            .pGlobalBarriers = global_barriers.data(),
        });
    }
    if (!image_barriers.empty())
    {
        d3d12_barrier_groups.emplace_back(D3D12_BARRIER_GROUP {
            .Type             = D3D12_BARRIER_TYPE_TEXTURE,
            .NumBarriers      = static_cast<u32>(image_barriers.size()),
            .pTextureBarriers = image_barriers.data(),
        });
    }
    if (!buffer_barriers.empty())
    {
        d3d12_barrier_groups.emplace_back(D3D12_BARRIER_GROUP {
            .Type            = D3D12_BARRIER_TYPE_BUFFER,
            .NumBarriers     = static_cast<u32>(buffer_barriers.size()),
            .pBufferBarriers = buffer_barriers.data(),
        });
    }

    gfx_command_list->Barrier(d3d12_barrier_groups.size(), d3d12_barrier_groups.data());
}

void rhi::d3d12_cmd_image_blit(command_buffer cmd, image src, image dst, const blit_image_info& info)
{
    ZoneScoped;
    const auto* d3d12_cmd       = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_src_image = cast_from_handle<platform::d3d12_image>(src);
    const auto* d3d12_dst_image = cast_from_handle<platform::d3d12_image>(dst);

    assert2(d3d12_cmd && d3d12_src_image && d3d12_dst_image);
    if (!d3d12_cmd || !d3d12_src_image || !d3d12_dst_image)
    {
        return;
    }
}

void rhi::d3d12_cmd_copy_buffer(command_buffer cmd, buffer src_buffer, const u64vec2 src_range, buffer dst_buffer,
                                const u64 dst_offset)
{
    ZoneScoped;
    const auto* d3d12_cmd        = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_src_buffer = cast_from_handle<platform::d3d12_buffer>(src_buffer);
    const auto* d3d12_dst_buffer = cast_from_handle<platform::d3d12_buffer>(dst_buffer);

    assert2(d3d12_cmd && d3d12_src_buffer && d3d12_dst_buffer);
    if (!d3d12_cmd || !d3d12_src_buffer || !d3d12_dst_buffer)
    {
        return;
    }

    if (const auto gfx_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_list->CopyBufferRegion(d3d12_dst_buffer->allocation->GetResource(),
                                   dst_offset,
                                   d3d12_src_buffer->allocation->GetResource(),
                                   src_range.x,
                                   src_range.y);
    }
}

void rhi::d3d12_cmd_clear_buffer(command_buffer cmd, buffer buffer, const u64vec2 range, const u32 value)
{
    ZoneScoped;
    const auto* d3d12_cmd    = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_buffer = cast_from_handle<platform::d3d12_buffer>(buffer);

    assert2(d3d12_cmd && d3d12_buffer);
    if (!d3d12_cmd || !d3d12_buffer)
    {
        return;
    }

    assert2m(d3d12_buffer->uav.cpu_handle.ptr, "buffer was not created with shader_rw usage");
    assert2m(d3d12_cmd->shader_visible_heaps[0], "buffer clears are unavailable on a copy command list");
    if (!d3d12_buffer->uav.cpu_handle.ptr || !d3d12_cmd->shader_visible_heaps[0])
    {
        return;
    }

    const u64 size = range.y == kBufferAll ? (d3d12_buffer->size - range.x) : range.y;
    assert2m(range.x % 4 == 0 && size % 4 == 0, "buffer clear range has to be 4 byte aligned");

    const u32 values[4] = {value, value, value, value};
    const D3D12_RECT rect {
        .left   = static_cast<LONG>(range.x / 4),
        .top    = 0,
        .right  = static_cast<LONG>((range.x + size) / 4),
        .bottom = 1,
    };

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ClearUnorderedAccessViewUint(d3d12_buffer->uav.gpu_handle,
                                                       d3d12_buffer->uav_cpu.cpu_handle,
                                                       d3d12_buffer->allocation->GetResource(),
                                                       values,
                                                       1,
                                                       &rect);
    }
}

void rhi::d3d12_cmd_update_buffer(command_buffer cmd, buffer buffer, const u64vec2 range, const void* data)
{
    ZoneScoped;
    auto* d3d12_cmd          = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_buffer = cast_from_handle<platform::d3d12_buffer>(buffer);

    assert2(d3d12_cmd && d3d12_buffer && data);
    if (!d3d12_cmd || !d3d12_buffer || !data)
    {
        return;
    }

    const u64 size   = range.y == kBufferAll ? (d3d12_buffer->size - range.x) : range.y;
    const u64 offset = platform::d3d12_command_list_scratch_alloc(*d3d12_cmd, size);

    if (offset >= platform::kD3D12_CommandListScratchSize)
    {
        return;
    }

    cpp::cx_memcpy(static_cast<u8*>(d3d12_cmd->scratch.mapped) + offset, data, size);
    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->CopyBufferRegion(d3d12_buffer->allocation->GetResource(),
                                           range.x,
                                           d3d12_cmd->scratch.allocation->GetResource(),
                                           offset,
                                           size);
    }
}

void rhi::d3d12_cmd_copy_buffer_to_image(command_buffer cmd, buffer src, image dst, const image_layout layout,
                                         const std::span<const copy_image_info> regions)
{
    ZoneScoped;
    const auto* d3d12_cmd    = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_buffer = cast_from_handle<platform::d3d12_buffer>(src);
    const auto* d3d12_image  = cast_from_handle<platform::d3d12_image>(dst);

    assert2(d3d12_cmd && d3d12_buffer && d3d12_image);
    if (!d3d12_cmd || !d3d12_buffer || !d3d12_image)
    {
        return;
    }

    assert2m(platform::d3d12_rhi_parse_image_layout(layout) == d3d12_image->layout,
             "image layout does not match the one it was last barriered into");

    const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd);
    if (!gfx_command_list)
    {
        return;
    }

    const auto resource_desc = d3d12_image->resource->GetDesc();
    const auto counts        = d3d12_rhi_subresource_counts(resource_desc);

    const auto ddspp_format = static_cast<ddspp::DXGIFormat>(resource_desc.Format);

    unsigned int block_w = 1;
    unsigned int block_h = 1;
    ddspp::get_block_size(ddspp_format, block_w, block_h);
    const u32 bytes_per_block = ddspp::get_bits_per_pixel_or_block(ddspp_format) / 8;

    for (const auto& info : regions)
    {
        const auto planes = platform::d3d12_rhi_parse_aspect_flags(info.aspects);

        const u32 first_layer = info.dst_layers.x;
        const u32 layer_count = info.dst_layers.y == rhi::kLayersAll ? (counts.y - first_layer) : info.dst_layers.y;

        const u32 rows      = (info.dst_extent.y + block_h - 1) / block_h;
        const u32 row_pitch = ((info.dst_extent.x + block_w - 1) / block_w) * bytes_per_block;

        const D3D12_TEXTURE_COPY_LOCATION src_location {
            .pResource       = d3d12_buffer->allocation->GetResource(),
            .Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = {.Offset    = info.source_offset,
                                .Footprint = {.Format   = resource_desc.Format,
                                              .Width    = info.dst_extent.x,
                                              .Height   = info.dst_extent.y,
                                              .Depth    = info.dst_extent.z,
                                              .RowPitch = row_pitch}},
        };

        for (u32 layer = 0; layer < layer_count; ++layer)
        {
            for (u32 plane = 0; plane < planes.y; ++plane)
            {
                const u32 subresource =
                    info.dst_mip + (first_layer + layer) * counts.x + (planes.x + plane) * counts.x * counts.y;

                const D3D12_TEXTURE_COPY_LOCATION dst_location {
                    .pResource        = d3d12_image->resource.Get(),
                    .Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                    .SubresourceIndex = subresource,
                };

                // every layer reads its own slice of the mip chain, same as vulkan walking layerCount
                auto layer_src = src_location;
                layer_src.PlacedFootprint.Offset += static_cast<u64>(layer) * row_pitch * rows;

                gfx_command_list->CopyTextureRegion(
                    &dst_location, info.dst_offset.x, info.dst_offset.y, info.dst_offset.z, &layer_src, nullptr);
            }
        }
    }
}

void rhi::d3d12_cmd_clear_color_attachment(command_buffer cmd, image image, const color_clear_value value)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_img = cast_from_handle<platform::d3d12_image>(image);

    assert2(d3d12_cmd && d3d12_img);
    if (!d3d12_cmd || !d3d12_img)
    {
        return;
    }

    assert2m(d3d12_img->view.view_types & platform::d3d12_image_view_type::rtv,
             "image was not created with color attachment usage");
    assert2m(d3d12_img->layout == D3D12_BARRIER_LAYOUT_RENDER_TARGET,
             "image has to be in the render target layout to be cleared");

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ClearRenderTargetView(d3d12_img->view.descriptors.rtv.cpu_handle, &value.f4.x, 0, nullptr);
    }
}

void rhi::d3d12_cmd_clear_depth_attachment(command_buffer cmd, image image, const ds_clear_value value)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_img = cast_from_handle<platform::d3d12_image>(image);

    assert2(d3d12_cmd && d3d12_img);
    if (!d3d12_cmd || !d3d12_img)
    {
        return;
    }

    assert2m(d3d12_img->view.view_types & platform::d3d12_image_view_type::dsv,
             "image was not created with depth stencil attachment usage");
    assert2m(d3d12_img->layout == D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
             "image has to be in the depth stencil write layout to be cleared");

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ClearDepthStencilView(d3d12_img->view.descriptors.dsv.cpu_handle,
                                                platform::d3d12_dsv_clear_flags(d3d12_img->view.desc.format),
                                                value.depth,
                                                value.stencil,
                                                0,
                                                nullptr);
    }
}

void rhi::d3d12_cmd_set_draw_state(command_buffer cmd, std::span<const attachment_state_info> color_attachments,
                                   attachment_state_info depth_attachment, uvec4 viewport)
{
    ZoneScoped;
    auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
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
        const auto* d3d12_view = d3d12_rhi_view_of(color_attachments[i].attachment);
        assert2(d3d12_view);
        if (!d3d12_view)
        {
            return;
        }

        color_handles[i] = d3d12_view->descriptors.rtv.cpu_handle;

        switch (color_attachments[i].load_op)
        {
        case resource_load_op::discard :
            gfx_command_list->DiscardResource(d3d12_view->source, nullptr);
            break;
        case resource_load_op::clear :
            gfx_command_list->ClearRenderTargetView(
                color_handles[i], &color_attachments[i].clear_value.color.f4.x, 0, nullptr);
            break;
        default :
        case resource_load_op::load :
            break;
        }
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE* depth_stencil = nullptr;
    if (depth_attachment)
    {
        const auto* d3d12_view = d3d12_rhi_view_of(depth_attachment.attachment);
        assert2(d3d12_view);
        if (!d3d12_view)
        {
            return;
        }

        depth_stencil = &d3d12_view->descriptors.dsv.cpu_handle;

        switch (depth_attachment.load_op)
        {
        case resource_load_op::discard :
            gfx_command_list->DiscardResource(d3d12_view->source, nullptr);
            break;
        case resource_load_op::clear :
            gfx_command_list->ClearDepthStencilView(*depth_stencil,
                                                    platform::d3d12_dsv_clear_flags(d3d12_view->desc.format),
                                                    depth_attachment.clear_value.ds.depth,
                                                    depth_attachment.clear_value.ds.stencil,
                                                    0,
                                                    nullptr);
            break;
        default :
        case resource_load_op::load :
            break;
        }
    }

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

void rhi::d3d12_cmd_clear_draw_state(command_buffer /* cmd */)
{
    ZoneScoped;
}

void rhi::d3d12_cmd_set_cull_mode(command_buffer cmd, const cull_mode mode)
{
    ZoneScoped;
    auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    assert2(d3d12_cmd);
    if (!d3d12_cmd || !d3d12_cmd->pso)
    {
        return;
    }

    d3d12_cmd->cull_mode = mode;
    d3d12_rhi_refresh_pso_bind(d3d12_cmd);
}

void rhi::d3d12_cmd_set_depth_bias(command_buffer cmd, const f32 constant_factor, const f32 slope_factor,
                                   const f32 clamp)
{
    ZoneScoped;
    auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    d3d12_cmd->depth_bias_params = vec3(constant_factor, clamp, slope_factor);
    d3d12_rhi_refresh_pso_bind(d3d12_cmd);
}

void rhi::d3d12_cmd_bind_pso(command_buffer cmd, pipeline pso)
{
    ZoneScoped;

    auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    auto* d3d12_pso = cast_from_handle<platform::d3d12_pipeline>(pso);
    if (!d3d12_cmd || !d3d12_pso)
    {
        return;
    }

    d3d12_cmd->pso               = d3d12_pso;
    d3d12_cmd->depth_bias_params = vec3(0.0F);
    d3d12_cmd->cull_mode         = rhi::cull_mode::none;

    d3d12_rhi_refresh_pso_bind(d3d12_cmd);
}

void rhi::d3d12_cmd_bind_index(command_buffer cmd, buffer index_buffer)
{
    ZoneScoped;
    const auto* d3d12_cmd          = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_index_buffer = cast_from_handle<platform::d3d12_buffer>(index_buffer);

    if (!d3d12_cmd || !d3d12_index_buffer)
    {
        return;
    }

    const D3D12_INDEX_BUFFER_VIEW d3d12_view {
        .BufferLocation = d3d12_index_buffer->va,
        .SizeInBytes    = static_cast<UINT>(d3d12_index_buffer->size),
        .Format         = DXGI_FORMAT_R32_UINT,
    };

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->IASetIndexBuffer(&d3d12_view);
    }
}

void rhi::d3d12_cmd_dispatch(command_buffer cmd, pipeline pso, const uvec3 threads)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_pso = cast_from_handle<platform::d3d12_pipeline>(pso);

    if (!d3d12_cmd || !d3d12_pso)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        const auto d3d12_threads = d3d12_align_wg3(threads, d3d12_pso->work_group_size);
        gfx_command_list->Dispatch(d3d12_threads.x, d3d12_threads.y, d3d12_threads.z);
    }
}

void rhi::d3d12_cmd_dispatch_indirect(command_buffer cmd, buffer count_buffer, const u32 buffer_offset)
{
    ZoneScoped;
    auto* d3d12_cmd              = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_cnt_buffer = cast_from_handle<platform::d3d12_buffer>(count_buffer);

    if (!d3d12_cmd || !d3d12_cnt_buffer)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ExecuteIndirect(
            d3d12_cmd->get_signature(
                {.stride = sizeof(u32[3]), .type = platform::d3d12_command_signature_type::dispatch_indirect}),
            1,
            d3d12_cnt_buffer->allocation->GetResource(),
            buffer_offset,
            nullptr,
            0);
    }
}

void rhi::d3d12_cmd_draw(command_buffer cmd, const u32 vtx_count, const u32 instance_count, const u32 first_vertex,
                         const u32 first_instance)
{
    ZoneScoped;
    const auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);

    assert2(d3d12_cmd);
    if (!d3d12_cmd)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->DrawInstanced(vtx_count, instance_count, first_vertex, first_instance);
    }
}

void rhi::d3d12_cmd_draw_indexed_indirect(command_buffer cmd, buffer buffer, const u64 offset, const u32 count,
                                          const u32 stride)
{
    ZoneScoped;
    auto* d3d12_cmd          = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_buffer = cast_from_handle<platform::d3d12_buffer>(buffer);

    assert2(stride < 0xFF);
    assert2(d3d12_cmd && d3d12_buffer);
    if (!d3d12_cmd || !d3d12_buffer)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ExecuteIndirect(
            d3d12_cmd->get_signature({.stride = static_cast<u8>(stride),
                                      .type   = platform::d3d12_command_signature_type::draw_indexed_indirect}),
            count,
            d3d12_buffer->allocation->GetResource(),
            offset,
            nullptr,
            0);
    }
}

void rhi::d3d12_cmd_draw_indexed_indirect_count(command_buffer cmd, rhi::buffer buffer, const u64 offset,
                                                rhi::buffer count_buffer, const u64 count_offset, const u32 max_count,
                                                const u32 stride)
{
    ZoneScoped;
    auto* d3d12_cmd              = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_buffer     = cast_from_handle<platform::d3d12_buffer>(buffer);
    const auto* d3d12_cnt_buffer = cast_from_handle<platform::d3d12_buffer>(count_buffer);

    assert2(stride < 0xFF);
    assert2(d3d12_cmd && d3d12_buffer && d3d12_cnt_buffer);
    if (!d3d12_cmd || !d3d12_buffer || !d3d12_cnt_buffer)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ExecuteIndirect(
            d3d12_cmd->get_signature({.stride = static_cast<u8>(stride),
                                      .type   = platform::d3d12_command_signature_type::draw_indexed_indirect}),
            max_count,
            d3d12_buffer->allocation->GetResource(),
            offset,
            d3d12_cnt_buffer->allocation->GetResource(),
            count_offset);
    }
}

void rhi::d3d12_cmd_draw_mesh_indirect(command_buffer cmd, buffer buffer, const u64 offset, const u32 count,
                                       const u32 stride)
{
    ZoneScoped;
    auto* d3d12_cmd          = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_buffer = cast_from_handle<platform::d3d12_buffer>(buffer);

    assert2(stride < 0xFF);
    assert2(d3d12_cmd && d3d12_buffer);
    if (!d3d12_cmd || !d3d12_buffer)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ExecuteIndirect(
            d3d12_cmd->get_signature({.stride = cpp::max<u8>(static_cast<u8>(stride), sizeof(u32[3])),
                                      .type   = platform::d3d12_command_signature_type::draw_mesh_task_indirect}),
            count,
            d3d12_buffer->allocation->GetResource(),
            offset,
            nullptr,
            0);
    }
}

void rhi::d3d12_cmd_draw_mesh_indirect_count(command_buffer cmd, rhi::buffer buffer, const u64 offset,
                                             rhi::buffer count_buffer, const u64 count_offset, const u32 max_count,
                                             const u32 stride)
{
    ZoneScoped;
    auto* d3d12_cmd              = cast_from_handle<platform::d3d12_command_list>(cmd);
    const auto* d3d12_buffer     = cast_from_handle<platform::d3d12_buffer>(buffer);
    const auto* d3d12_cnt_buffer = cast_from_handle<platform::d3d12_buffer>(count_buffer);

    assert2(stride < 0xFF);
    assert2(d3d12_cmd && d3d12_buffer && d3d12_cnt_buffer);
    if (!d3d12_cmd || !d3d12_buffer || !d3d12_cnt_buffer)
    {
        return;
    }

    if (const auto gfx_command_list = d3d12_rhi_get_gfx_command_list(d3d12_cmd))
    {
        gfx_command_list->ExecuteIndirect(
            d3d12_cmd->get_signature({.stride = static_cast<u8>(stride),
                                      .type   = platform::d3d12_command_signature_type::draw_mesh_task_indirect}),
            max_count,
            d3d12_buffer->allocation->GetResource(),
            offset,
            d3d12_cnt_buffer->allocation->GetResource(),
            count_offset);
    }
}

void rhi::d3d12_cmd_debug_label_begin(command_buffer cmd, context context, const debug_label& label)
{
    ZoneScoped;
    // TODO: pix3.h
}

void rhi::d3d12_cmd_debug_label_end(command_buffer cmd, context context)
{
    ZoneScoped;
    // TODO: pix3.h
    // https://github.com/microsoft/PixEvents/blob/main/include/pix3.h
}

void rhi::d3d12_submit(context context, const queue queue, const submit_info& info)
{
    auto* d3d12_ctx   = cast_from_handle<platform::d3d12_context>(context);
    auto* d3d12_queue = d3d12_rhi_object_from_handle<ID3D12CommandQueue>(queue);

    assert2(d3d12_ctx && d3d12_queue);
    if (!d3d12_ctx || !d3d12_queue)
    {
        return;
    }

    for (const auto& wait : info.waits)
    {
        const auto d3d12_fence = cast_from_handle<platform::d3d12_fence>(wait.fence);
        if (!d3d12_fence)
        {
            continue;
        }

        platform::d3d12_queue_wait_on_fence(*d3d12_fence, d3d12_queue, wait.value);
    }

    cpp::local_array<ID3D12CommandList*> command_lists;
    for (const auto& cmd : info.command_buffers)
    {
        const auto d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);
        if (!d3d12_cmd)
        {
            continue;
        }

        command_lists.emplace_back(d3d12_cmd->command_list.Get());
    }

    d3d12_queue->ExecuteCommandLists(static_cast<u32>(command_lists.size()), command_lists.data());

    for (const auto& signal : info.signals)
    {
        const auto d3d12_fence = cast_from_handle<platform::d3d12_fence>(signal.fence);
        if (!d3d12_fence)
        {
            continue;
        }

        platform::d3d12_signal_fence(*d3d12_fence, d3d12_queue, signal.value);
    }
}

void rhi::d3d12_present(command_buffer cmd, swapchain swapchain, const queue submit, const queue present)
{
    ZoneScoped;
    auto* d3d12_sc        = cast_from_handle<platform::d3d12_swapchain>(swapchain);
    auto* d3d12_queue     = d3d12_rhi_object_from_handle<ID3D12CommandQueue>(submit);
    const auto* d3d12_cmd = cast_from_handle<platform::d3d12_command_list>(cmd);

    assert2(d3d12_queue == d3d12_rhi_object_from_handle<ID3D12CommandQueue>(present));
    if (!d3d12_cmd || !d3d12_sc)
    {
        return;
    }

    ID3D12CommandList* const command_lists[] = {d3d12_cmd->command_list.Get()};
    d3d12_queue->ExecuteCommandLists(COUNT_OF(command_lists), command_lists);

    const auto bb_index      = d3d12_sc->swapchain->GetCurrentBackBufferIndex();
    const auto sync_interval = d3d12_sc->flags & platform::swapchain_flag::vsync ? 1 : 0;
    const auto present_flags =
        d3d12_sc->flags & platform::swapchain_flag::tearing && !sync_interval ? DXGI_PRESENT_ALLOW_TEARING : 0;

    D3D12_ASSERT_ON_FAIL(d3d12_sc->swapchain->Present(sync_interval, present_flags));
    d3d12_signal_fence(d3d12_sc->sc_sync_fence, d3d12_queue, ++d3d12_sc->frame_counter);
    d3d12_sc->back_buffers[bb_index].wait_value = d3d12_sc->frame_counter;
}

auto rhi::d3d12_ffx_create_upscale_context(context ctx, const ffx_upscale_desc& desc) -> result<ffx_context>
{
    ZoneScoped;
    const auto* d3d12_ctx = cast_from_handle<platform::d3d12_context>(ctx);

    assert2(d3d12_ctx);
    if (!d3d12_ctx)
    {
        return error("failed to access context");
    }

    auto d3d12_ffx_ctx = platform::d3d12_ffx_create_upscale_context(d3d12_ctx->device.Get(), desc);
    RESULT_FORWARD_IF_FAILED(d3d12_ffx_ctx);

    return create_handle<ffx_context>(*d3d12_ffx_ctx);
}

void rhi::d3d12_ffx_destroy_context(ffx_context& ffx_ctx)
{
    ZoneScoped;
    auto* d3d12_ffx_ctx = cast_from_handle<ffx::Context>(ffx_ctx);

    assert2(d3d12_ffx_ctx);
    if (d3d12_ffx_ctx)
    {
        ffx::DestroyContext(*d3d12_ffx_ctx);
        clear_handle(d3d12_ffx_ctx, ffx_ctx);
    }
}

auto rhi::d3d12_ffx_upscale_query_jitter(ffx_context ffx_ctx, const u32 render_width, const u32 display_width,
                                         const i32 jitter_index) -> result<vec2>
{
    ZoneScoped;
    const auto* d3d12_ffx_ctx = cast_from_handle<ffx::Context>(ffx_ctx);

    assert2(d3d12_ffx_ctx);
    if (!d3d12_ffx_ctx)
    {
        return error("failed to access context");
    }

    return platform::d3d12_ffx_get_jitter_offset(*d3d12_ffx_ctx, render_width, display_width, jitter_index);
}

void rhi::d3d12_ffx_upscale_dispatch(ffx_context ffx_ctx, command_buffer cmd, const ffx_upscale_params& params,
                                     const ffx_upscale_resources& resources)
{
    ZoneScoped;
    const auto* d3d12_ffx_ctx = cast_from_handle<ffx::Context>(ffx_ctx);
    const auto* d3d12_cmd     = cast_from_handle<platform::d3d12_command_list>(cmd);

    assert2(d3d12_ffx_ctx && d3d12_cmd);
    if (!d3d12_ffx_ctx || !d3d12_cmd)
    {
        return;
    }

    auto query = [](const rhi::image attachment) -> FfxApiResource
    {
        if (attachment == null_image)
        {
            return {};
        }

        return platform::d3d12_ffx_get_resource_wrapper(*cast_from_handle<platform::d3d12_image>(attachment));
    };

    const platform::ffx_upscale_resources d3d12_resources {
        .color                        = query(resources.color),
        .depth                        = query(resources.depth),
        .motion_vectors               = query(resources.motion_vectors),
        .output                       = query(resources.output),
        .reactive                     = query(resources.reactive),
        .transparency_and_composition = query(resources.transparency_and_composition),
    };
    platform::d3d12_ffx_dispatch_upscale(*d3d12_ffx_ctx, d3d12_cmd->command_list.Get(), params, d3d12_resources);
}

#endif
