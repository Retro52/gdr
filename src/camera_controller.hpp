#pragma once

#include <types.hpp>

#include <events.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>

// @imgui
struct camera_controller
{
    f32 move_speed       = 5.0f;
    f32 look_sensitivity = 0.002f;

    /// @hide
    f32 yaw = 0.0f;
    /// @hide
    f32 pitch = 0.0f;

    camera_controller(events_queue& queue, entity& camera)
        : m_camera(camera)
        , m_queue(queue)
    {
        m_queue.add_watcher(
            event_type::request_draw,
            [](const event_payload&, void* user_data)
            {
                ZoneScopedN("camera_controller: request_draw watcher");
                camera_controller& self = *static_cast<camera_controller*>(user_data);
                if (self.m_queue.get_mouse_button_state(mouse_button::left) != button_state::down)
                {
                    return;
                }

                self.m_queue.center_cursor();
            },
            this);

        m_queue.add_watcher(
            event_type::mouse_move,
            +[](const event_payload& payload, void* user_data)
            {
                ZoneScopedN("camera_controller: mouse_move watcher");
                camera_controller& self = *static_cast<camera_controller*>(user_data);
                if (self.m_queue.get_mouse_button_state(mouse_button::left) != button_state::down)
                {
                    return;
                }

                self.yaw -= payload.mouse.delta.x * self.look_sensitivity;
                self.pitch -= payload.mouse.delta.y * self.look_sensitivity;

                auto& camera_transform = self.m_camera.get_component<transform_component>();
                self.update_rotation(camera_transform);
            },
            this);

        m_queue.add_watcher(
            event_type::mouse_pressed,
            [](const event_payload& payload, void* user_data)
            {
                ZoneScopedN("camera_controller: mouse_pressed watcher");
                if (payload.mouse.button == mouse_button::left)
                {
                    static_cast<events_queue*>(user_data)->hide_cursor();
                }
            },
            &m_queue);

        m_queue.add_watcher(
            event_type::mouse_released,
            [](const event_payload& payload, void* user_data)
            {
                ZoneScopedN("camera_controller: mouse_released watcher");
                if (payload.mouse.button == mouse_button::left)
                {
                    static_cast<events_queue*>(user_data)->show_cursor();
                }
            },
            &m_queue);
    }

    void update_rotation(transform_component& transform)
    {
        ZoneScoped;

        pitch = glm::clamp(pitch, -glm::half_pi<f32>() + 0.01f, glm::half_pi<f32>() - 0.01f);

        glm::quat quat_yaw   = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat quat_pitch = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        transform.rotation = quat_yaw * quat_pitch;
    }

    void update_position(transform_component& transform, const camera_component& cam, const f32 dt) const
    {
        ZoneScoped;
        if (m_queue.get_mouse_button_state(mouse_button::left) != button_state::down)
        {
            return;
        }

        const glm::vec3 up      = cam.get_up(transform.rotation);
        const glm::vec3 forward = cam.get_direction(transform.rotation);
        const glm::vec3 right   = glm::normalize(glm::cross(forward, up));

        auto velocity = glm::vec3(0.0f);

        velocity += up * (m_queue.key_down(keycode::e) ? 1.0F : (m_queue.key_down(keycode::q) ? -1.0F : 0.0F));
        velocity += right * (m_queue.key_down(keycode::d) ? 1.0F : (m_queue.key_down(keycode::a) ? -1.0F : 0.0F));
        velocity += forward * (m_queue.key_down(keycode::w) ? 1.0F : (m_queue.key_down(keycode::s) ? -1.0F : 0.0F));

        if (glm::length(velocity) > 0.0f)
        {
            velocity = glm::normalize(velocity) * move_speed * dt;
            transform.position += velocity;
        }
    }

private:
    /// @hide
    entity& m_camera;
    /// @hide
    events_queue& m_queue;
};
