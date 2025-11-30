#include <scene/entity.hpp>
#include <scene/scene.hpp>

entity scene::create_entity()
{
    return entity(m_registry.create(), m_registry);
}

void scene::delete_entity(entity& entity)
{
    m_registry.destroy(entity.m_entity);
}
