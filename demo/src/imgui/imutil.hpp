#pragma once

#include <pod_types.hpp>

namespace ImGuiUtil
{
    // Dimmed hint text, centered horizontally in the available region
    void TextCenteredDisabled(const char* text);

    // Push cursor down so the next block of `block_height` pixels is vertically centered
    void VerticalCentering(f32 block_height);

    // Center the next item of `item_width` horizontally
    void HorizontalCentering(f32 item_width);
}
