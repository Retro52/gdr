#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_fence.hpp>

auto platform::d3d12_create_fence(ID3D12Device* device, const u64 initial_value) -> result<d3d12_fence>
{
    d3d12_fence result;

    D3D12_RETURN_ON_FAIL(device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&result.fence)));
    result.event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

    return result;
}

void platform::d3d12_signal_fence(const d3d12_fence& fence, ID3D12CommandQueue* queue, const u64 value)
{
    if (!queue || !fence.fence)
    {
        return;
    }

    D3D12_ASSERT_ON_FAIL(queue->Signal(fence.fence.Get(), value));
}

void platform::d3d12_wait_on_fence(const d3d12_fence& fence, const u64 value, const u64 timeout)
{
    if (!fence.fence || fence.fence->GetCompletedValue() < value)
    {
        D3D12_ASSERT_ON_FAIL(fence.fence->SetEventOnCompletion(value, fence.event));
        ::WaitForSingleObject(fence.event, static_cast<DWORD>(timeout));
    }
}

void platform::d3d12_destroy_fence(d3d12_fence& fence)
{
    fence.fence.Reset();
    ::CloseHandle(fence.event);
    fence.event = nullptr;
}
