#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <scene/matrix_common.hpp>

glm::vec3 scene::get_up(const glm::quat& rotation) noexcept
{
    constexpr vec3 up = vec3(0.0F, 1.0F, 0.0F);
    return glm::mat3_cast(rotation) * up;
}

glm::vec3 scene::get_direction(const glm::quat& rotation) noexcept
{
    constexpr vec3 front = vec3(0.0F, 0.0F, -1.0F);
    return glm::mat3_cast(rotation) * front;
}

glm::mat4 scene::get_projection_matrix(const f32 near, const f32 ratio, const f32 fov) noexcept
{
    // https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
    // clang-format off
    const float f = 1.0F / glm::tan(fov / 2.0F);
    return glm::mat4(
        f / ratio, 0.0F, 0.0F, 0.0F,
        0.0F, -f, 0.0F, 0.0F,
        0.0F, 0.0F, 0.0F, -1.0F,
        0.0F, 0.0F, near, 0.0F);
    // clang-format on
}

glm::mat4 scene::get_projection_matrix(const f32 near, const f32 far, const f32 ratio, const f32 fov) noexcept
{
    // https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
    // clang-format off
    const float f = 1.0F / glm::tan(fov / 2.0F);
    return glm::mat4(
        f / ratio, 0.0F, 0.0F, 0.0F,
        0.0F, -f, 0.0F, 0.0F,
        0.0F, 0.0F, near / (far - near), -1.0F,
        0.0F, 0.0F, (near * far) / (far - near), 0.0F);
    // clang-format on
}

glm::mat4 scene::get_view_matrix(const vec3& position, const glm::quat& rotation) noexcept
{
    return glm::inverse(glm::translate(glm::mat4(1.0F), position) * glm::mat4_cast(rotation));
}
