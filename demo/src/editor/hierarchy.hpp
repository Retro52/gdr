#pragma once

class scene;
struct scene_hierarchy;

#include <pod_types.hpp>

namespace editor
{
    struct hierarchy_window_context
    {
        void draw(scene& scene);

    private:
        void draw_node(scene& scene, u32 node_idx);

    private:
        u32 m_selected_node = 0;
    };
}
