#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <scene/components.hpp>
#include <scene/matrix_common.hpp>

transform_component::transform_component(const glm::mat4& transform)
{
    glm::vec3 scale, skew;
    glm::vec4 perspective;

    glm::decompose(transform, scale, this->rotation, this->position, skew, perspective);
    this->uniform_scale = glm::max(scale.x, glm::max(scale.y, scale.z));
}

transform_component::transform_component(const vec3& position, float uniform_scale, const glm::quat& rotation)
    : position(position)
    , uniform_scale(uniform_scale)
    , rotation(rotation)
{
}

glm::vec3 camera_component::get_up(const glm::quat& rotation) const noexcept
{
    return scene::get_up(rotation);
}

glm::vec3 camera_component::get_direction(const glm::quat& rotation) const noexcept
{
    return scene::get_direction(rotation);
}

glm::mat4 camera_component::get_projection_matrix() const noexcept
{
    return scene::get_projection_matrix(this->near_plane, this->aspect_ratio, this->horizontal_fov);
}

glm::mat4 camera_component::get_projection_matrix(const f32 far_plane) const noexcept
{
    return scene::get_projection_matrix(this->near_plane, far_plane, this->aspect_ratio, this->horizontal_fov);
}

glm::mat4 camera_component::get_view_matrix(const vec3& position, const glm::quat& rotation) noexcept
{
    return scene::get_view_matrix(position, rotation);
}
