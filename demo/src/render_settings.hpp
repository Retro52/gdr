#pragma once

#include <types.hpp>

/// @imgui
struct render_settings
{
    f32 render_distance {10'000.0F};

    /// @hide
    u32 flags {0xFFFF};
};
