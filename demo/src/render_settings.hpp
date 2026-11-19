#pragma once

#include <types.hpp>

/// @imgui
struct render_settings
{
    f32 render_distance {10'000.0F};

    /// @hide
    u32 flags {0xFF7F};  // everything but kPrevFrameHiZBBit (bitfields are the default visibility source)
};
