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

    template<typename... Components>
    auto get_view() const
    {
        return m_registry.view<Components...>();
    }

    template<typename T>
    [[nodiscard]] T& get_component(entt::entity entity)
    {
        return m_registry.get<T>(entity);
    }

    template<typename T>
    [[nodiscard]] bool has_component(entt::entity entity) const
    {
        return m_registry.all_of<T>(entity);
    }

    template<typename T, typename... Args>
    T& add_component(entt::entity entity, Args&&... args) const
    {
        return m_registry.emplace<T>(entity, std::forward<Args>(args)...);
    }

private:
    entt::registry m_registry;
};
