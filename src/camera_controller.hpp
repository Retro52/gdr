#pragma once

#include <types.hpp>

#include <events.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>

// @imgui
struct camera_controller
{
    f32 base_move_speed  = 25.0f;
    f32 look_sensitivity = 0.002f;

    f32 yaw   = 0.0f;
    f32 roll  = 0.0f;
    f32 pitch = 0.0f;

    camera_controller(events_queue& queue, entity& camera)
        : m_camera(camera)
        , m_queue(queue)
    {
        m_queue.add_watcher(
            event_type::mouse_scroll,
            [](const event_payload& payload, void* user_data)
            {
                ZoneScopedN("camera_controller: mouse_scroll watcher");
                camera_controller& self = *static_cast<camera_controller*>(user_data);
                if (self.m_queue.get_mouse_button_state(mouse_button::left) != button_state::down)
                {
                    return;
                }

                self.base_move_speed += payload.scroll.delta.y;
            },
            this);

        m_queue.add_watcher(
            event_type::key_pressed,
            [](const event_payload& payload, void* user_data)
            {
                ZoneScopedN("camera_controller: mouse_scroll watcher");
                if (payload.keyboard.key == keycode::x)
                {
                    camera_controller& self = *static_cast<camera_controller*>(user_data);
                    self.roll               = 0.0F;
                    auto& camera_transform  = self.m_camera.get_component<transform_component>();
                    self.update_rotation(camera_transform);
                }
            },
            this);

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

                const f32 cos_roll = std::cos(-self.roll);
                const f32 sin_roll = std::sin(-self.roll);

                const f32 adjusted_dx = payload.mouse.delta.x * cos_roll - payload.mouse.delta.y * sin_roll;
                const f32 adjusted_dy = payload.mouse.delta.x * sin_roll + payload.mouse.delta.y * cos_roll;

                self.yaw -= adjusted_dx * self.look_sensitivity;
                self.pitch -= adjusted_dy * self.look_sensitivity;

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
                    camera_controller& self = *static_cast<camera_controller*>(user_data);
                    self.m_queue.hide_cursor();
                }
            },
            this);

        m_queue.add_watcher(
            event_type::mouse_released,
            [](const event_payload& payload, void* user_data)
            {
                ZoneScopedN("camera_controller: mouse_released watcher");
                if (payload.mouse.button == mouse_button::left)
                {
                    camera_controller& self = *static_cast<camera_controller*>(user_data);
                    self.m_queue.show_cursor();
                }
            },
            this);
    }

    ~camera_controller()
    {
        m_queue.remove_watcher(this);
    }

    void update(transform_component& transform, const camera_component& cam, const f32 dt)
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
            auto speed = base_move_speed;
            speed *= m_queue.key_down(keycode::sc_ctrl) ? 0.2F : 1.0F;
            speed *= m_queue.key_down(keycode::sc_shift) ? 5.0F : 1.0F;

            velocity = glm::normalize(velocity) * speed * dt;
            transform.position += velocity;
        }

        if (m_queue.get_key_state(keycode::z) == button_state::down
            || m_queue.get_key_state(keycode::c) == button_state::down)
        {
            const f32 dir = m_queue.get_key_state(keycode::z) == button_state::down ? +1.0F : -1.0F;
            roll += dir * dt;
            update_rotation(transform);
        }
    }

    void update_rotation(transform_component& transform)
    {
        ZoneScoped;

        pitch = glm::clamp(pitch, -glm::half_pi<f32>() + 0.01f, glm::half_pi<f32>() - 0.01f);

        glm::quat quat_yaw   = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat quat_roll  = glm::angleAxis(roll, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::quat quat_pitch = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        transform.rotation = quat_yaw * quat_pitch * quat_roll;
    }

private:
    /// @hide
    entity& m_camera;
    /// @hide
    events_queue& m_queue;
};
