#pragma once

#include <types.hpp>

#include <events_queue.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>

// @imgui
struct camera_controller
{
    f32 move_speed       = 5.0f;
    f32 look_sensitivity = 0.003f;

    /// @hide
    f32 yaw = 0.0f;
    /// @hide
    f32 pitch = 0.0f;

    // FIXME:
    // * 1. Make IsKeyPressed/IsKeyDown as part of an API
    // * 2. Rework events_queue API to support multiple callbacks (via c-stype, w/o std::function?)
    /// @hide
    bool key_w = false;
    /// @hide
    bool key_a = false;
    /// @hide
    bool key_s = false;
    /// @hide
    bool key_d = false;
    /// @hide
    bool key_e = false;
    /// @hide
    bool key_q = false;
    /// @hide
    bool mouse_pressed = false;

    void bind(events_queue& queue, entity& camera)
    {
        queue.set_watcher(event_type::mouse_pressed,
                          [&](const events_queue::event_payload& payload)
                          {
                              if (payload.mouse.button == mouse_button::right)
                              {
                                  this->mouse_pressed = true;
                              }
                          });

        queue.set_watcher(event_type::mouse_released,
                          [&](const events_queue::event_payload& payload)
                          {
                              if (payload.mouse.button == mouse_button::right)
                              {
                                  this->mouse_pressed = false;
                              }
                          });

        queue.set_watcher(event_type::mouse_move,
                          [&](const events_queue::event_payload& payload)
                          {
                              if (!this->mouse_pressed)
                              {
                                  return;
                              }

                              this->yaw -= payload.mouse.delta.x * this->look_sensitivity;
                              this->pitch -= payload.mouse.delta.y * this->look_sensitivity;

                              auto& camera_transform = camera.get_component<transform_component>();
                              this->update_rotation(camera_transform);
                          });

        queue.set_watcher(event_type::key_pressed,
                          [&](const events_queue::event_payload& payload)
                          {
                              switch (payload.keyboard.key)
                              {
                              case keycode::w :
                                  this->key_w = true;
                                  break;
                              case keycode::a :
                                  this->key_a = true;
                                  break;
                              case keycode::s :
                                  this->key_s = true;
                                  break;
                              case keycode::d :
                                  this->key_d = true;
                                  break;
                              case keycode::e :
                                  this->key_e = true;
                                  break;
                              case keycode::q :
                                  this->key_q = true;
                                  break;
                              default :
                                  break;
                              }
                          });

        queue.set_watcher(event_type::key_released,
                          [&](const events_queue::event_payload& payload)
                          {
                              switch (payload.keyboard.key)
                              {
                              case keycode::w :
                                  this->key_w = false;
                                  break;
                              case keycode::a :
                                  this->key_a = false;
                                  break;
                              case keycode::s :
                                  this->key_s = false;
                                  break;
                              case keycode::d :
                                  this->key_d = false;
                                  break;
                              case keycode::e :
                                  this->key_e = false;
                                  break;
                              case keycode::q :
                                  this->key_q = false;
                                  break;
                              default :
                                  break;
                              }
                          });
    }

    void update_rotation(transform_component& transform)
    {
        pitch = glm::clamp(pitch, -glm::half_pi<f32>() + 0.01f, glm::half_pi<f32>() - 0.01f);

        glm::quat quat_yaw   = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat quat_pitch = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        transform.rotation = quat_yaw * quat_pitch;
    }

    void update_position(transform_component& transform, const camera_component& cam, f32 dt)
    {
        glm::vec3 up      = cam.get_up(transform.rotation);
        glm::vec3 forward = cam.get_direction(transform.rotation);
        glm::vec3 right   = glm::normalize(glm::cross(forward, up));

        glm::vec3 velocity(0.0f);

        if (key_w)
        {
            velocity += forward;
        }
        if (key_s)
        {
            velocity -= forward;
        }
        if (key_d)
        {
            velocity += right;
        }
        if (key_a)
        {
            velocity -= right;
        }
        if (key_e)
        {
            velocity += up;
        }
        if (key_q)
        {
            velocity -= up;
        }

        if (glm::length(velocity) > 0.0f)
        {
            velocity = glm::normalize(velocity) * move_speed * dt;
            transform.position += velocity;
        }
    }
};
