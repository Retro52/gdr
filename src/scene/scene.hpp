#pragma once

#include <entt/entt.hpp>

class entity;

class scene
{
public:
    entity create_entity();

    void delete_entity(entity& entity);

    template<typename... Components>
    auto get_view()
    {
        return m_registry.view<Components...>();
    }

private:
    entt::registry m_registry;
};
