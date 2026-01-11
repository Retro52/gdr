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
    // https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
    // clang-format off
    const float f = 1.0F / glm::tan(this->horizontal_fov / 2.0F);
    return glm::mat4(
        f / this->aspect_ratio, 0.0F, 0.0F, 0.0F,
        0.0F, -f, 0.0F, 0.0F,
        0.0F, 0.0F, 0.0F, -1.0F,
        0.0F, 0.0F, this->near_plane, 0.0F);
    // clang-format on
}

glm::mat4 camera_component::get_view_matrix(const vec3& position, const glm::quat& rotation) const noexcept
{
    return glm::inverse(glm::translate(glm::mat4(1.0F), position) * glm::mat4_cast(rotation));
}
