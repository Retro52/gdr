#pragma once

#include <types.hpp>

#include <cpp/string/stack_string.hpp>

#include <atomic>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <render/static_model.hpp>

/// @imgui
struct transform_component
{
    glm::quat rotation;
    vec3 position;
    vec3 scale {1, 1, 1};
};

/// @imgui
struct static_model_component
{
    static_model model;
};

/// @imgui
struct directional_light_component
{
    /// @color
    vec3 color;

    /// @color
    vec3 direction;
};

/// @imgui
struct id_component
{
    /// @readonly
    const u64 id {id_counter++};
    DEBUG_ONLY(cpp::stack_string name);

    id_component() = default;
    DEBUG_ONLY(id_component(const char* name) : name(name) {});

private:
    /// @hide
    inline static std::atomic<u64> id_counter;
};

/// @imgui
struct camera_component
{
    f32 far_plane;
    f32 near_plane;

    f32 aspect_ratio;
    /// @rad2deg
    f32 horizontal_fov;

    // These below are utility functions, that need some info from a transform component, like position/rotation
    [[nodiscard]] glm::vec3 get_up(const glm::quat& rotation) const noexcept;
    [[nodiscard]] glm::vec3 get_direction(const glm::quat& rotation) const noexcept;

    [[nodiscard]] glm::mat4 get_projection_matrix() const noexcept;
    [[nodiscard]] glm::mat4 get_view_matrix(const vec3& position, const glm::quat& rotation) const noexcept;
};
