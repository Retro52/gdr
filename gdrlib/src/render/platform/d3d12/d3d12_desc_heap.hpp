#pragma once

#include <cpp/containers/heap_array.hpp>
#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

#include <map>

namespace platform
{
    struct d3d12_desc_heap;

    struct d3d12_view_desc
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {};
    };

    struct d3d12_view_desc_range
    {
        u64 count  = 0;
        u64 stride = 0;
        d3d12_view_desc start;

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpu_at(const u64 index) const noexcept
        {
            assert2m(index < count, "index out of range");
            return {.ptr = this->start.cpu_handle.ptr + this->stride * index};
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE gpu_at(const u64 index) const noexcept
        {
            assert2m(index < count, "index out of range");
            assert2m(start.gpu_handle.ptr > 0, "desc heap is not shader visible");

            return {.ptr = this->start.gpu_handle.ptr + this->stride * index};
        }
    };

    struct d3d12_desc_heap
    {
    public:
        using index_t = u32;

        com_ptr<ID3D12DescriptorHeap> heap;

        UINT stride = 0;
        d3d12_view_desc start;

        d3d12_desc_heap() = default;

        explicit d3d12_desc_heap(const u32 count) { m_free_ranges[0] = count; }

    private:
        void split_free_range(const index_t base, const u32 count)
        {
            const auto it = m_free_ranges.find(base);
            assert2(it != m_free_ranges.end());

            const u32 range_count = it->second;
            m_free_ranges.erase(it);

            if (range_count > count)
            {
                m_free_ranges[base + count] = range_count - count;
            }
        }

        [[nodiscard]] index_t alloc_internal(const u32 count)
        {
            for (auto [range_base, range_count] : m_free_ranges)
            {
                if (range_count >= count)
                {
                    split_free_range(range_base, count);
                    return range_base;
                }
            }

            assert2m(false, "out of free ranges");
            return static_cast<index_t>(-1);
        }

        void free_internal(const index_t base, const u32 count)
        {
            const index_t end                   = base + count;
            const auto it_merge_candidate_right = m_free_ranges.find(end);

            auto it_merge_candidate_left = m_free_ranges.lower_bound(base);
            if (it_merge_candidate_left != m_free_ranges.begin())
            {
                --it_merge_candidate_left;
            }
            else
            {
                it_merge_candidate_left = m_free_ranges.end();
            }

            decltype(m_free_ranges)::iterator dst;
            if (it_merge_candidate_left != m_free_ranges.end()
                && it_merge_candidate_left->first + it_merge_candidate_left->second == base)
            {
                dst                             = it_merge_candidate_left;
                it_merge_candidate_left->second = end - it_merge_candidate_left->first;
            }
            else
            {
                dst = m_free_ranges.emplace(base, count).first;
            }

            if (it_merge_candidate_right != m_free_ranges.end())
            {
                dst->second += it_merge_candidate_right->second;
                m_free_ranges.erase(it_merge_candidate_right);
            }
        }

    public:
        d3d12_view_desc alloc(const u32 count = 1)
        {
            const index_t alloc_index = alloc_internal(count);
            return {
                .cpu_handle = {.ptr = this->start.cpu_handle.ptr + this->stride * alloc_index},
                .gpu_handle = {.ptr = this->start.gpu_handle.ptr
                                        ? this->start.gpu_handle.ptr + this->stride * alloc_index
                                        : 0},
            };
        }

        void free(const d3d12_view_desc desc, const u32 count = 1)
        {
            const index_t alloc_index = (desc.cpu_handle.ptr - this->start.cpu_handle.ptr) / this->stride;
            free_internal(alloc_index, count);
        }

    private:
        std::map<index_t, u32> m_free_ranges;
    };

    result<d3d12_desc_heap> d3d12_create_desc_heap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                   u32 num_descriptors);

    void d3d12_destroy_desc_heap(d3d12_desc_heap& heap);
}
