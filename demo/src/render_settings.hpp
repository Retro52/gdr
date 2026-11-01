#pragma once

#include <types.hpp>

/// @imgui
struct render_settings
{
    f32 render_distance {1'000.0F};

    /// @hide
    u32 flags {0xFFFF};
};
