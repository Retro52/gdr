#pragma once

#include <core/containers/heap_array.hpp>
#include <entt/entt.hpp>

struct scene_hierarchy
{
    static constexpr u32 kInvalidIndex = ~0U;

    struct node
    {
        entt::entity e;
        u32 parent;                      // kInvalidIndex for roots
        core::heap_array<u32> children;  // indices into `nodes`
    };

    core::heap_array<u32> roots;
    core::heap_array<node> nodes;
};
