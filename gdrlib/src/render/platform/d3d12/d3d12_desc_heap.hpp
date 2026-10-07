#pragma once

#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

#include <array>

namespace render
{
    struct d3d12_desc_heap
    {
        com_ptr<ID3D12DescriptorHeap> heap;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
        UINT cpu_increment_size;
    };

    template<u64 N>
    struct d3d12_tracked_heap
    {
        constexpr static u32 kEnd      = N;
        constexpr static u32 kOccupied = ~0U;

        d3d12_desc_heap heap;

        u32 next_free           = 0;
        std::array<u32, N> list = []
        {
            std::array<u32, N> result;

            result[N - 1] = 0;
            for (u32 i = 0; i < N - 1; i++)
            {
                result[i] = i + 1;
            }

            return result;
        }();

        D3D12_CPU_DESCRIPTOR_HANDLE alloc()
        {
            u32 result = next_free;
            assert2(result != kEnd);

            next_free    = list[next_free];
            list[result] = kOccupied;
            return D3D12_CPU_DESCRIPTOR_HANDLE {heap.cpu_handle.ptr + heap.cpu_increment_size * result};
        }

        void free(const D3D12_CPU_DESCRIPTOR_HANDLE handle)
        {
            assert2(handle.ptr >= heap.cpu_handle.ptr);
            assert2((handle.ptr - heap.cpu_handle.ptr) % heap.cpu_increment_size == 0);

            u32 index = (handle.ptr - heap.cpu_handle.ptr) / heap.cpu_increment_size;

            assert2(index < N);
            assert2(list[index] == kOccupied);

            list[index] = next_free;
            next_free   = index;
        }
    };

    template<u64 N>
    d3d12_tracked_heap<N> d3d12_track_heap(d3d12_desc_heap&& heap)
    {
        return {std::move(heap), {}};
    }

    result<d3d12_desc_heap> d3d12_create_desc_heap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                   u32 num_descriptors);

    void d3d12_destroy_desc_heap(d3d12_desc_heap& heap);
}
