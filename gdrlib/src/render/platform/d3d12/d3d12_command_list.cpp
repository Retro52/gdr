#include <render/platform/d3d12/d3d12_command_list.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>

auto render::d3d12_create_command_list(ID3D12Device4* device, const D3D12_COMMAND_LIST_TYPE type)
    -> result<d3d12_command_list>
{
    ZoneScoped;
    d3d12_command_list result;

    D3D12_RETURN_ON_FAIL(device->CreateCommandAllocator(type, IID_PPV_ARGS(&result.allocator)));
    D3D12_RETURN_ON_FAIL(
        device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&result.command_list)));
    return result;
}

void render::d3d12_destroy_command_list(d3d12_command_list& command_list)
{
    command_list.allocator.Reset();
    command_list.command_list.Reset();
}
