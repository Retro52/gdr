#pragma once

#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <result.hpp>

namespace render
{
    using d3d12_queue = com_ptr<ID3D12CommandQueue>;
    result<d3d12_queue> create_queue(const render::d3d12_context& ctx, D3D12_COMMAND_LIST_TYPE type);

    void destroy_queue(d3d12_queue& queue);
}
