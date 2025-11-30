#pragma once

#include <types.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <render/static_model.hpp>

struct transform_component
{
    vec3 scale;
    vec3 position;
    glm::quat rotation;
};

struct static_model_component
{
    static_model model;
};

struct directional_light_component
{
    vec3 color;
    vec3 direction;
};

struct camera_component
{
    f32 far_plane;
    f32 near_plane;

    f32 aspect_ratio;
    f32 horizontal_fov;

    [[nodiscard]] glm::vec3 get_up(const glm::quat& rotation) const noexcept;
    [[nodiscard]] glm::vec3 get_direction(const glm::quat& rotation) const noexcept;

    [[nodiscard]] glm::mat4 get_projection_matrix() const noexcept;
    [[nodiscard]] glm::mat4 get_view_matrix(const vec3& position, const glm::quat& rotation) const noexcept;
};
