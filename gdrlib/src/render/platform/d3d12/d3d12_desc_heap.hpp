#pragma once

#include <cpp/containers/heap_array.hpp>
#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

namespace platform
{
    struct d3d12_desc_heap
    {
    public:
        com_ptr<ID3D12DescriptorHeap> heap;

        UINT stride;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;

        using index_t = u16;

    public:
        [[nodiscard]] index_t alloc()
        {
            if (m_free_list.empty())
            {
                return m_next_free++;
            }

            const auto free = m_free_list.back();
            m_free_list.pop_back();

            return free;
        }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE alloc_cpu() { return to_cpu(this->alloc()); }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE alloc_gpu() { return to_gpu(this->alloc()); }

        void free(const index_t idx) { m_free_list.push_back(idx); }

        void free_cpu(const D3D12_CPU_DESCRIPTOR_HANDLE handle) { free(from_cpu(handle)); }

        void free_gpu(const D3D12_GPU_DESCRIPTOR_HANDLE handle) { free(from_gpu(handle)); }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE to_cpu(const index_t slot) const
        {
            return {this->cpu_handle.ptr + slot * stride};
        }

        [[nodiscard]] index_t from_cpu(const D3D12_CPU_DESCRIPTOR_HANDLE slot) const
        {
            return (this->cpu_handle.ptr - slot.ptr) / stride;
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE to_gpu(const index_t slot) const
        {
            return {this->gpu_handle.ptr + slot * stride};
        }

        [[nodiscard]] index_t from_gpu(const D3D12_GPU_DESCRIPTOR_HANDLE slot) const
        {
            return (this->gpu_handle.ptr - slot.ptr) / stride;
        }

    private:
        index_t m_next_free = 0;
        cpp::heap_array<index_t> m_free_list;
    };

    result<d3d12_desc_heap> d3d12_create_desc_heap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                   u32 num_descriptors);

    void d3d12_destroy_desc_heap(d3d12_desc_heap& heap);
}
