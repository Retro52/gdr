#pragma once

#include <types.hpp>

/// @imgui
struct render_settings
{
    f32 render_distance {10'000.0F};

    f32 shadow_cascade_blend {0.1F};
    f32 shadow_normal_offset {1.5F};
    f32 shadow_shader_bias {0.0005F};
    f32 shadow_depth_bias_slope {-2.75F};
    f32 shadow_depth_bias_constant {-1.25F};

    /// @hide
    u32 flags {0xFFFF};
};
