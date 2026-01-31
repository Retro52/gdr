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

    /// @readonly @name(Tris/s (B))
    f64 tris_from_max {0.0F};

    /// @readonly
    u64 tris_in_scene_max {0};

    /// @readonly
    u64 tris_in_scene_total {0};

    void update(f64 start, f64 end, u64 tris_count, u64 tris_max)
    {
        frame_start = start;
        frame_end   = end;

        tris_in_scene_max   = tris_max;
        tris_in_scene_total = tris_count;

        tris_from_max = static_cast<f64>(tris_in_scene_total) / static_cast<f64>(tris_in_scene_max);

        // smooth the averages over time
        constexpr f32 kSmoothingFactor = 0.9F;

        gpu_render_time = gpu_render_time == 0.0F
                            ? (end - start)
                            : (gpu_render_time * kSmoothingFactor + (end - start) * (1.0F - kSmoothingFactor));

        // to convert it to billions we'd use 1e-9, but we also convert ms to s, so 1e-3 cancel out
        tris_per_second = static_cast<f64>(tris_count) * 1e-6 / (gpu_render_time);
    }
};
