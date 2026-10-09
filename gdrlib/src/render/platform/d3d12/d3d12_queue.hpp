#pragma once

#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

namespace platform
{
    using d3d12_queue = com_ptr<ID3D12CommandQueue>;
    result<d3d12_queue> d3d12_create_queue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);

    void d3d12_destroy_queue(d3d12_queue& queue);
}
