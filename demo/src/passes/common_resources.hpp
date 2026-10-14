#pragma once

#include <app/gpu_upload_mgr.hpp>
#include <cpp/containers/heap_array.hpp>
#include <render/rhi.hpp>

namespace app
{
    struct descriptor_bindings
    {
        constexpr static u32 kMaxSetZeroBindings = 32;
        u32 max_count                            = 0;
        rhi::binding bindings[kMaxSetZeroBindings] {};

        std::span<rhi::binding> get() { return {bindings, max_count}; }

        auto& bind_next(const rhi::binding& next, const bool condition = true)
        {
            if (!condition)
            {
                return *this;
            }

            assert2(max_count < kMaxSetZeroBindings);
            bindings[max_count] = next;
            max_count           = cpp::min(kMaxSetZeroBindings, max_count + 1);
            return *this;
        }

        auto& bind_at(const rhi::binding& next, const u32 index, const bool condition = true)
        {
            if (!condition)
            {
                return *this;
            }

            assert2(index < kMaxSetZeroBindings);
            bindings[index] = next;
            max_count       = cpp::min(kMaxSetZeroBindings, cpp::max(max_count, index + 1));
            return *this;
        }
    };

    struct common_resources
    {
    private:
        u32 m_frame_id         = 0;
        u32 m_frames_in_flight = 0;

    public:
        common_resources(const rhi::impl& rhi, rhi::context ctx, u32 frames_in_flight);

        void next_frame();

        // world geometry
        rhi::buffer vertex;
        rhi::buffer meshlets;
        rhi::buffer primitives;
        rhi::buffer instances;
        rhi::buffer materials;
        rhi::buffer meshlets_payload;

        rhi::sampler bindless_sampler;
        rhi::bindless_set bindless_textures;

        cpp::heap_array<mapped_buffer> world_data_buffers;
        cpp::heap_array<mapped_buffer> frame_cull_data_buffers;
        cpp::heap_array<mapped_buffer> motion_vector_data_buffers;

        [[nodiscard]] mapped_buffer& world_data_buffer() { return world_data_buffers[m_frame_id]; }

        [[nodiscard]] mapped_buffer& frame_cull_data_buffer() { return frame_cull_data_buffers[m_frame_id]; }

        [[nodiscard]] mapped_buffer& motion_vector_data_buffer() { return motion_vector_data_buffers[m_frame_id]; }

        [[nodiscard]] const mapped_buffer& world_data_buffer() const { return world_data_buffers[m_frame_id]; }

        [[nodiscard]] const mapped_buffer& frame_cull_data_buffer() const
        {
            return frame_cull_data_buffers[m_frame_id];
        }

        [[nodiscard]] const mapped_buffer& motion_vector_data_buffer() const
        {
            return motion_vector_data_buffers[m_frame_id];
        }
    };
}
