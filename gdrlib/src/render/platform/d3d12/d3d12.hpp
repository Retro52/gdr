#pragma once

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

// clang-format off
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <D3D12MemAlloc.h>
// clang-format on

#include <wrl/client.h>

namespace render
{
    template<typename T>
    using com_ptr = Microsoft::WRL::ComPtr<T>;
}
