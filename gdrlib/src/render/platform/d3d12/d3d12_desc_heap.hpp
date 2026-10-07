#pragma once

#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

#include <array>
#include <type_traits>

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
        static_assert(N > 0 && N < 0xFFFF, "descriptor heap too large for an inline free list");
        using index_t = std::conditional_t<N < 0xFF, u8, u16>;

        constexpr static index_t kEnd      = N;
        constexpr static index_t kOccupied = ~static_cast<index_t>(0);

        d3d12_desc_heap heap;

        index_t next_free           = 0;
        std::array<index_t, N> list = []
        {
            std::array<index_t, N> result;

            result[N - 1] = kEnd;
            for (index_t i = 0; i < N - 1; ++i)
            {
                result[i] = i + 1;
            }

            return result;
        }();

        D3D12_CPU_DESCRIPTOR_HANDLE alloc()
        {
            index_t result = next_free;
            assert2(result != kEnd);

            next_free    = list[next_free];
            list[result] = kOccupied;
            return D3D12_CPU_DESCRIPTOR_HANDLE {heap.cpu_handle.ptr + heap.cpu_increment_size * result};
        }

        void free(const D3D12_CPU_DESCRIPTOR_HANDLE handle)
        {
            assert2(handle.ptr >= heap.cpu_handle.ptr);

            const u64 offset = handle.ptr - heap.cpu_handle.ptr;

            assert2(offset / heap.cpu_increment_size < N);
            assert2(offset % heap.cpu_increment_size == 0);

            index_t index = offset / heap.cpu_increment_size;

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
