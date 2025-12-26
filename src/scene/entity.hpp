#pragma once

#include <detail.hpp>
#include <entt/entt.hpp>

class entity
{
public:
    template<typename T, typename... Args>
    T& add_component(Args&&... args) const
    {
        return m_registry.emplace<T>(m_entity, std::forward<Args>(args)...);
    }

    template<typename T>
    [[nodiscard]] bool has_component() const
    {
        return m_registry.all_of<T>(m_entity);
    }

    template<typename T>
    [[nodiscard]] T& get_component() const
    {
        return m_registry.get<T>(m_entity);
    }

    template<typename... Components>
    entity shallow_clone(const entity& source)
    {
        entity clone(source.m_registry.create(), source.m_registry);
        detail::for_each_type<std::tuple<Components...>>(
            [&]<typename T>()
            {
                if (this->has_component<T>())
                {
                    clone.add_component<T>(this->get_component<T>());
                }
            });

        return clone;
    }

private:
    friend class scene;

    explicit entity(entt::entity entity, entt::registry& registry)
        : m_entity(entity)
        , m_registry(registry)
    {
    }

private:
    entt::entity m_entity;
    entt::registry& m_registry;
};
