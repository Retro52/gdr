#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>
#include <tracy/Tracy.hpp>

static DXGI_FORMAT d3d12_srv_format(const DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_D16_UNORM :
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_D24_UNORM_S8_UINT :
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT :
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT :
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default :
        return format;
    }
}

static D3D12_SHADER_RESOURCE_VIEW_DESC d3d12_make_srv_desc(const platform::d3d12_image_view_desc& desc)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC d3d12_desc {
        .Format                  = d3d12_srv_format(desc.format),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };

    switch (desc.kind)
    {
    case rhi::image_view_kind::flat_1d :
        d3d12_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
        d3d12_desc.Texture1D     = {.MostDetailedMip = desc.mips.x, .MipLevels = desc.mips.y};
        break;
    case rhi::image_view_kind::array_1d :
        d3d12_desc.ViewDimension  = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
        d3d12_desc.Texture1DArray = {
            .MostDetailedMip = desc.mips.x,
            .MipLevels       = desc.mips.y,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
        };
        break;
    case rhi::image_view_kind::flat_cube :
        d3d12_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        d3d12_desc.TextureCube   = {.MostDetailedMip = desc.mips.x, .MipLevels = desc.mips.y};
        break;
    case rhi::image_view_kind::array_cube :
        d3d12_desc.ViewDimension    = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        d3d12_desc.TextureCubeArray = {
            .MostDetailedMip  = desc.mips.x,
            .MipLevels        = desc.mips.y,
            .First2DArrayFace = desc.layers.x,
            .NumCubes         = desc.layers.y / 6,
        };
        break;
    case rhi::image_view_kind::flat_3d :
        d3d12_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        d3d12_desc.Texture3D     = {.MostDetailedMip = desc.mips.x, .MipLevels = desc.mips.y};
        break;
    case rhi::image_view_kind::flat_2d :
        d3d12_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        d3d12_desc.Texture2D     = {.MostDetailedMip = desc.mips.x, .MipLevels = desc.mips.y};
        break;
    default :
    case rhi::image_view_kind::array_2d :
        d3d12_desc.ViewDimension  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        d3d12_desc.Texture2DArray = {
            .MostDetailedMip = desc.mips.x,
            .MipLevels       = desc.mips.y,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
        };
        break;
    }

    return d3d12_desc;
}

// uav, rtv & dsv can only see a single mip, and there's no such thing as a cube for them
static D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12_make_uav_desc(const platform::d3d12_image_view_desc& desc)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12_desc {.Format = d3d12_srv_format(desc.format)};
    switch (desc.kind)
    {
    case rhi::image_view_kind::flat_1d :
        d3d12_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
        d3d12_desc.Texture1D     = {.MipSlice = desc.mips.x};
        break;
    case rhi::image_view_kind::array_1d :
        d3d12_desc.ViewDimension  = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
        d3d12_desc.Texture1DArray = {
            .MipSlice        = desc.mips.x,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
        };
        break;
    case rhi::image_view_kind::flat_3d :
        d3d12_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        d3d12_desc.Texture3D     = {.MipSlice = desc.mips.x, .FirstWSlice = 0, .WSize = ~0U};
        break;
    case rhi::image_view_kind::flat_2d :
        d3d12_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        d3d12_desc.Texture2D     = {.MipSlice = desc.mips.x, .PlaneSlice = 0};
        break;
    default :
    case rhi::image_view_kind::flat_cube :
    case rhi::image_view_kind::array_cube :
    case rhi::image_view_kind::array_2d :
        d3d12_desc.ViewDimension  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        d3d12_desc.Texture2DArray = {
            .MipSlice        = desc.mips.x,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
            .PlaneSlice      = 0,
        };
        break;
    }

    return d3d12_desc;
}

static D3D12_RENDER_TARGET_VIEW_DESC d3d12_make_rtv_desc(const platform::d3d12_image_view_desc& desc)
{
    D3D12_RENDER_TARGET_VIEW_DESC d3d12_desc {.Format = desc.format};
    switch (desc.kind)
    {
    case rhi::image_view_kind::flat_1d :
        d3d12_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
        d3d12_desc.Texture1D     = {.MipSlice = desc.mips.x};
        break;
    case rhi::image_view_kind::array_1d :
        d3d12_desc.ViewDimension  = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
        d3d12_desc.Texture1DArray = {
            .MipSlice        = desc.mips.x,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
        };
        break;
    case rhi::image_view_kind::flat_3d :
        d3d12_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        d3d12_desc.Texture3D     = {.MipSlice = desc.mips.x, .FirstWSlice = 0, .WSize = ~0U};
        break;
    case rhi::image_view_kind::flat_2d :
        d3d12_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        d3d12_desc.Texture2D     = {.MipSlice = desc.mips.x, .PlaneSlice = 0};
        break;
    default :
    case rhi::image_view_kind::array_2d :
    case rhi::image_view_kind::flat_cube :
    case rhi::image_view_kind::array_cube :
        d3d12_desc.ViewDimension  = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        d3d12_desc.Texture2DArray = {
            .MipSlice        = desc.mips.x,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
            .PlaneSlice      = 0,
        };
        break;
    }

    return d3d12_desc;
}

static D3D12_DEPTH_STENCIL_VIEW_DESC d3d12_make_dsv_desc(const platform::d3d12_image_view_desc& desc)
{
    D3D12_DEPTH_STENCIL_VIEW_DESC d3d12_desc {.Format = desc.format, .Flags = D3D12_DSV_FLAG_NONE};
    switch (desc.kind)
    {
    case rhi::image_view_kind::flat_1d :
        d3d12_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
        d3d12_desc.Texture1D     = {.MipSlice = desc.mips.x};
        break;
    case rhi::image_view_kind::array_1d :
        d3d12_desc.ViewDimension  = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
        d3d12_desc.Texture1DArray = {
            .MipSlice        = desc.mips.x,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
        };
        break;
    case rhi::image_view_kind::flat_2d :
        d3d12_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        d3d12_desc.Texture2D     = {.MipSlice = desc.mips.x};
        break;
    default :
        d3d12_desc.ViewDimension  = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        d3d12_desc.Texture2DArray = {
            .MipSlice        = desc.mips.x,
            .FirstArraySlice = desc.layers.x,
            .ArraySize       = desc.layers.y,
        };
        break;
    }

    // ReSharper disable once CppSomeObjectMembersMightNotBeInitialized
    return d3d12_desc;
}

static void d3d12_create_srv_if_needed(ID3D12Device* device, ID3D12Resource* source, platform::d3d12_image_view& dst,
                                       platform::d3d12_desc_heap& heap, const platform::d3d12_image_view_desc& desc,
                                       const platform::d3d12_image_view_type_bits flags)
{
    if (!(flags & platform::d3d12_image_view_type::srv))
    {
        return;
    }

    platform::d3d12_view_desc* dst_desc = &dst.descriptors.srv;
    if (flags & platform::d3d12_image_view_type::dsv) [[unlikely]]
    {
        dst_desc = &dst.descriptors.dsv_srv;
    }

    *dst_desc             = heap.alloc();
    const auto d3d12_desc = d3d12_make_srv_desc(desc);
    device->CreateShaderResourceView(source, &d3d12_desc, dst_desc->cpu_handle);
}

static void d3d12_create_dsv_if_needed(ID3D12Device* device, ID3D12Resource* source, platform::d3d12_image_view& dst,
                                       platform::d3d12_desc_heap& heap, const platform::d3d12_image_view_desc& desc,
                                       const platform::d3d12_image_view_type_bits flags)
{
    if (!(flags & platform::d3d12_image_view_type::dsv))
    {
        return;
    }

    dst.descriptors.dsv   = heap.alloc();
    const auto d3d12_desc = d3d12_make_dsv_desc(desc);
    device->CreateDepthStencilView(source, &d3d12_desc, dst.descriptors.dsv.cpu_handle);
}

static void d3d12_create_uav_if_needed(ID3D12Device* device, ID3D12Resource* source, platform::d3d12_image_view& dst,
                                       platform::d3d12_desc_heap& heap, const platform::d3d12_image_view_desc& desc,
                                       const platform::d3d12_image_view_type_bits flags)
{
    if (!(flags & platform::d3d12_image_view_type::uav))
    {
        return;
    }

    assert2(!(flags & platform::d3d12_image_view_type::dsv));

    dst.descriptors.uav   = heap.alloc();
    const auto d3d12_desc = d3d12_make_uav_desc(desc);
    device->CreateUnorderedAccessView(source, nullptr, &d3d12_desc, dst.descriptors.uav.cpu_handle);
}

static void d3d12_create_rtv_if_needed(ID3D12Device* device, ID3D12Resource* source, platform::d3d12_image_view& dst,
                                       platform::d3d12_desc_heap& heap, const platform::d3d12_image_view_desc& desc,
                                       const platform::d3d12_image_view_type_bits flags)
{
    if (!(flags & platform::d3d12_image_view_type::rtv))
    {
        return;
    }

    assert2(!(flags & platform::d3d12_image_view_type::dsv));

    dst.descriptors.rtv   = heap.alloc();
    const auto d3d12_desc = d3d12_make_rtv_desc(desc);
    device->CreateRenderTargetView(source, &d3d12_desc, dst.descriptors.rtv.cpu_handle);
}

static void d3d12_free_view_if_needed(platform::d3d12_desc_heap& heap, platform::d3d12_view_desc& view)
{
    if (view.cpu_handle.ptr)
    {
        heap.free(view);
        view = {};
    }
}

auto platform::d3d12_create_image_view(ID3D12Device* device, ID3D12Resource* source, const d3d12_heaps_view& heaps,
                                       const d3d12_image_view_desc& desc, const d3d12_image_view_type_bits flags)
    -> result<d3d12_image_view>
{
    ZoneScoped;
    d3d12_image_view result {.source = source, .desc = desc, .view_types = flags};

    d3d12_create_dsv_if_needed(device, source, result, heaps.dsv_heap, desc, flags);
    d3d12_create_rtv_if_needed(device, source, result, heaps.rtv_heap, desc, flags);
    d3d12_create_uav_if_needed(device, source, result, heaps.cbv_srv_uav_heap, desc, flags);
    d3d12_create_srv_if_needed(device, source, result, heaps.cbv_srv_uav_heap, desc, flags);

    return result;
}

auto platform::d3d12_view_types_from_resource_flags(const D3D12_RESOURCE_FLAGS flags) -> d3d12_image_view_type_bits
{
    d3d12_image_view_type_bits result = d3d12_image_view_type::srv;

    result = (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? (result | d3d12_image_view_type::rtv) : result;
    result = (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ? (result | d3d12_image_view_type::dsv) : result;
    result = (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ? (result | d3d12_image_view_type::uav) : result;

    return result;
}

void platform::d3d12_write_srv(ID3D12Device* device, const d3d12_image_view& view,
                               const D3D12_CPU_DESCRIPTOR_HANDLE dst)
{
    ZoneScoped;
    assert2(view.source && (view.view_types & d3d12_image_view_type::srv));
    if (!view.source)
    {
        return;
    }

    const auto d3d12_desc = d3d12_make_srv_desc(view.desc);
    device->CreateShaderResourceView(view.source, &d3d12_desc, dst);
}

void platform::d3d12_write_uav(ID3D12Device* device, const d3d12_image_view& view,
                               const D3D12_CPU_DESCRIPTOR_HANDLE dst)
{
    ZoneScoped;
    assert2(view.source && (view.view_types & d3d12_image_view_type::uav));
    if (!view.source)
    {
        return;
    }

    const auto d3d12_desc = d3d12_make_uav_desc(view.desc);
    device->CreateUnorderedAccessView(view.source, nullptr, &d3d12_desc, dst);
}

void platform::d3d12_destroy_image_view(d3d12_image_view& view, const d3d12_heaps_view& heaps)
{
    if (view.view_types & d3d12_image_view_type::dsv)
    {
        d3d12_free_view_if_needed(heaps.dsv_heap, view.descriptors.dsv);
        d3d12_free_view_if_needed(heaps.cbv_srv_uav_heap, view.descriptors.dsv_srv);
    }
    else
    {
        d3d12_free_view_if_needed(heaps.rtv_heap, view.descriptors.rtv);
        d3d12_free_view_if_needed(heaps.cbv_srv_uav_heap, view.descriptors.srv);
        d3d12_free_view_if_needed(heaps.cbv_srv_uav_heap, view.descriptors.uav);
    }
}

auto platform::d3d12_create_image(D3D12MA::Allocator* allocator, const D3D12_RESOURCE_DIMENSION type,
                                  const DXGI_FORMAT format, const u32 mips, const uvec3 dimensions,
                                  const D3D12_RESOURCE_FLAGS flags, rhi::clear_value optimized_clear_value)
    -> result<d3d12_image>
{
    ZoneScoped;
    d3d12_image result {.layout = D3D12_BARRIER_LAYOUT_COMMON};

    const D3D12_RESOURCE_DESC buf_desc {
        .Dimension        = type,
        .Width            = dimensions.x,
        .Height           = dimensions.y,
        .DepthOrArraySize = static_cast<UINT16>(dimensions.z),
        .MipLevels        = static_cast<UINT16>(mips),
        .Format           = format,
        .SampleDesc       = {.Count = 1, .Quality = 0},
        .Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags            = flags,
    };

    constexpr D3D12MA::ALLOCATION_DESC ma_desc = {.HeapType = D3D12_HEAP_TYPE_DEFAULT};

    const D3D12_CLEAR_VALUE* p_cv    = nullptr;
    D3D12_CLEAR_VALUE op_clear_value = {.Format = format};
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        p_cv                        = &op_clear_value;
        op_clear_value.DepthStencil = {
            .Depth   = optimized_clear_value.ds.depth,
            .Stencil = optimized_clear_value.ds.stencil,
        };
    }
    else if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        p_cv = &op_clear_value;
        cpp::cx_copy_n(&op_clear_value.Color[0], &optimized_clear_value.color.f4.x, 4);
    }

    D3D12_RETURN_ON_FAIL(allocator->CreateResource(
        &ma_desc, &buf_desc, D3D12_RESOURCE_STATE_COMMON, p_cv, &result.allocation, IID_PPV_ARGS(&result.resource)));

    return result;
}

void platform::d3d12_destroy_image(d3d12_image& image)
{
    ZoneScoped;
    image.resource.Reset();
    image.allocation.Reset();

    image.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
}
