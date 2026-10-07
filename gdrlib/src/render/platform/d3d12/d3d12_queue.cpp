#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_queue.hpp>
#include <tracy/Tracy.hpp>

auto render::d3d12_create_queue(ID3D12Device* device, const D3D12_COMMAND_LIST_TYPE type) -> result<d3d12_queue>
{
    ZoneScoped;

    d3d12_queue queue;
    const D3D12_COMMAND_QUEUE_DESC desc = {
        .Type     = type,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };

    D3D12_RETURN_ON_FAIL(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)));
    return queue;
}

void render::d3d12_destroy_queue(d3d12_queue& queue)
{
    queue.Reset();
}
