#pragma once

#include <detail.hpp>
#include <entt/entt.hpp>
#include <tracy/Tracy.hpp>

#include <functional>

class entity
{
public:
    template<typename T, typename... Args>
    void add_component(Args&&... args) const
    {
        ZoneScoped;
        m_registry.get().emplace<T>(m_entity, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& emplace_component(Args&&... args) const
    {
        ZoneScoped;
        return m_registry.get().emplace<T>(m_entity, std::forward<Args>(args)...);
    }

    template<typename T>
    [[nodiscard]] bool has_component() const
    {
        ZoneScoped;
        return m_registry.get().all_of<T>(m_entity);
    }

    template<typename T>
    [[nodiscard]] T& get_component() const
    {
        ZoneScoped;
        return m_registry.get().get<T>(m_entity);
    }

    template<typename... Components>
    [[nodiscard]] entity shallow_clone(const entity& source)
    {
        ZoneScoped;
        entity clone(source.m_registry.get().create(), source.m_registry);
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

    [[nodiscard]] entt::entity get_native() const noexcept { return m_entity; }

    [[nodiscard]] operator bool() const noexcept { return m_entity != entt::null; }

    [[nodiscard]] bool operator==(const entt::entity id) const noexcept { return m_entity == id; }

    [[nodiscard]] bool operator==(const entity& other) const noexcept
    {
        return m_entity == other.m_entity && &m_registry == &other.m_registry;
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
    std::reference_wrapper<entt::registry> m_registry;
};
