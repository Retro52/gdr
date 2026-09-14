#pragma once

#include <cpp/containers/heap_array.hpp>
#include <entt/entt.hpp>

struct scene_hierarchy
{
    static constexpr u32 kInvalidIndex = ~0U;

    struct node
    {
        entt::entity e;
        u32 parent;                     // kInvalidIndex for roots
        cpp::heap_array<u32> children;  // indices into `nodes`
    };

    cpp::heap_array<u32> roots;
    cpp::heap_array<node> nodes;
};
