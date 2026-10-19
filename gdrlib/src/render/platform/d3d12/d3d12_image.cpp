#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>
#include <tracy/Tracy.hpp>

auto platform::d3d12_create_image(D3D12MA::Allocator* allocator, const D3D12_RESOURCE_DIMENSION type,
                                  const DXGI_FORMAT format, const u32 mips, const uvec3 dimensions,
                                  const D3D12_RESOURCE_FLAGS flags) -> result<d3d12_image>
{
    ZoneScoped;
    d3d12_image result;

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

    D3D12_RETURN_ON_FAIL(allocator->CreateResource(
        &ma_desc, &buf_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, &result.allocation, IID_PPV_ARGS(&result.resource)));

    return result;
}

void platform::d3d12_destroy_image(d3d12_image& image)
{
    ZoneScoped;
    image.resource.Reset();
    image.resource.Reset();

    image.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
}
