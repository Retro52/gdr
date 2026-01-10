#include <cpp/math.hpp>
#include <scene/components.hpp>

glm::vec3 camera_component::get_up(const glm::quat& rotation) const noexcept
{
    constexpr vec3 up = vec3(0.0F, 1.0F, 0.0F);
    return glm::mat3_cast(rotation) * up;
}

glm::vec3 camera_component::get_direction(const glm::quat& rotation) const noexcept
{
    constexpr vec3 front = vec3(0.0F, 0.0F, -1.0F);
    return glm::mat3_cast(rotation) * front;
}

glm::mat4 camera_component::get_projection_matrix() const noexcept
{
    // GLM works in OpenGL clip space, and, you'd assume, Khronos would be consistent across their APIs, but no, for
    // whatever reason Y coordinate is flipped in Vulkan
    constexpr glm::mat4 kFlipMatrix {
        vec4(1, 0, 0, 0),
        vec4(0, -1, 0, 0),
        vec4(0, 0, 1, 0),
        vec4(0, 0, 0, 1),
    };
    return kFlipMatrix * glm::perspectiveZO(horizontal_fov, aspect_ratio, near_plane, far_plane);
}

glm::mat4 camera_component::get_view_matrix(const vec3& position, const glm::quat& rotation) const noexcept
{
    return glm::inverse(glm::translate(glm::mat4(1.0F), position) * glm::mat4_cast(rotation));
}
