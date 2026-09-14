#include <scene/entity.hpp>
#include <scene/scene.hpp>

entity scene::empty()
{
    return entity(entt::null, m_registry);
}

entity scene::create_entity()
{
    return entity(m_registry.create(), m_registry);
}

entity scene::create_ref(const entt::entity id)
{
    assert2(m_registry.valid(id));
    return entity(id, m_registry);
}

void scene::delete_entity(entity& entity)
{
    m_registry.destroy(entity.m_entity);
}
