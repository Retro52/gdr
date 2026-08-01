#pragma once

#include <types.hpp>

#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>

namespace scene
{
    glm::vec3 get_up(const glm::quat& rotation) noexcept;
    glm::vec3 get_direction(const glm::quat& rotation) noexcept;
    glm::mat4 get_projection_matrix(f32 near, f32 ratio, f32 fov) noexcept;
    glm::mat4 get_projection_matrix(f32 near, f32 far, f32 ratio, f32 fov) noexcept;
    glm::mat4 get_view_matrix(const vec3& position, const glm::quat& rotation) noexcept;
}
