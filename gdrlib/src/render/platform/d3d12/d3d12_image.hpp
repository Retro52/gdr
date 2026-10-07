#pragma once

#include <render/platform/d3d12/d3d12.hpp>

namespace render
{
    struct d3d12_image
    {
        com_ptr<ID3D12Resource> resource       = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        D3D12_RESOURCE_STATES current_state    = D3D12_RESOURCE_STATE_COMMON;
    };
}
