#pragma once

#include <types.hpp>

/// @imgui
struct gpu_profile_data
{
    /// @readonly
    f64 frame_start {0.0F};

    /// @readonly
    f64 frame_end {0.0F};

    /// @readonly @name(GPU render time (ms))
    f64 gpu_render_time {0.0F};

    /// @readonly @name(Tris/s (B))
    f64 tris_per_second {0.0F};

    /// @readonly @name(Tris/Meshlet (B))
    f64 tris_per_meshlet {0.0F};

    void update(f64 start, f64 end, u64 tris_count, u64 meshlets_count)
    {
        frame_start     = start;
        frame_end       = end;

        // smooth the averages over time
        gpu_render_time = gpu_render_time == 0.0F ? (end - start) : (gpu_render_time * 0.99 + (end - start) * 0.01);

        // to convert it to billions we'd use 1e-9, but we also convert ms to s, so 1e-3 cancel out
        tris_per_second = static_cast<f64>(tris_count) * 1e-6 / (gpu_render_time);

        tris_per_meshlet = static_cast<f64>(tris_count) / meshlets_count;
    }
};
