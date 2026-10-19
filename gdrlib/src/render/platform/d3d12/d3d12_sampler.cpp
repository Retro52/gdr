#include <render/platform/d3d12/d3d12_sampler.hpp>

auto platform::d3d12_create_sampler(ID3D12Device* device, d3d12_desc_heap& desc_heap, const D3D12_FILTER filter,
                                    const D3D12_TEXTURE_ADDRESS_MODE address_mode, const u32 anisotropic_filter,
                                    const D3D12_COMPARISON_FUNC compare_op, const vec4 border_color) -> d3d12_sampler
{
    const auto alloc_handle = desc_heap.alloc();

    d3d12_sampler result {};
    result.cpu_handle = desc_heap.to_cpu(alloc_handle);
    result.gpu_handle = desc_heap.to_gpu(alloc_handle);

    const D3D12_SAMPLER_DESC desc {
        .Filter         = filter,
        .AddressU       = address_mode,
        .AddressV       = address_mode,
        .AddressW       = address_mode,
        .MipLODBias     = 0,
        .MaxAnisotropy  = anisotropic_filter,
        .ComparisonFunc = compare_op,
        .BorderColor    = {border_color.r, border_color.g, border_color.b, border_color.a},
        .MinLOD         = 0.0F,
        .MaxLOD         = 16.0F,
    };

    device->CreateSampler(&desc, result.cpu_handle);
    return result;
}

void platform::d3d12_destroy_sampler(d3d12_desc_heap& desc_heap, d3d12_sampler& sampler)
{
    desc_heap.free(desc_heap.from_cpu(sampler.cpu_handle));
    memset(&sampler, 0, sizeof(sampler));
}
