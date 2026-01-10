#include <SDL3/SDL.h>

#include <backends/imgui_impl_sdl3.h>
#include <events_queue.hpp>

#define EQ_CHECK_WINDOW_ID(ID) \
    if (ID != m_window_id)     \
        return;

void populate_mouse_event_payload(events_queue::event_payload& payload, SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_BUTTON_UP :
    case SDL_EVENT_MOUSE_BUTTON_DOWN :
        payload.mouse.button = static_cast<mouse_button>(event.button.button);
        break;
    default :
        break;
    }

    payload.mouse.delta = {event.motion.xrel, event.motion.yrel};
    payload.mouse.pos   = {event.motion.x, event.motion.y};
}

events_queue::events_queue(const window& window)
    : m_window_id(SDL_GetWindowID(window.get_native_handle().window))
{
    SDL_InitSubSystem(SDL_INIT_EVENTS);

    auto filter = [](void* userdata, SDL_Event* event) -> bool
    {
        if (event && event->type == SDL_EVENT_WINDOW_EXPOSED)
        {
            static_cast<events_queue*>(userdata)->push_event(*event);
        }

        return true;
    };

    SDL_AddEventWatch(static_cast<SDL_EventFilter>(filter), this);
}

events_queue::~events_queue()
{
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

void events_queue::poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        push_event(event);
    }

    constexpr event_payload draw_event {.type = event_type::request_draw};
    send_event(draw_event);
}

void events_queue::push_event(SDL_Event& event)
{
    static event_payload payload;
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN :
        EQ_CHECK_WINDOW_ID(event.key.windowID);
        payload.type           = event_type::key_pressed;
        payload.keyboard.state = button_state::down;
        payload.keyboard.key   = static_cast<keycode>(event.key.scancode);
        break;
    case SDL_EVENT_KEY_UP :
        EQ_CHECK_WINDOW_ID(event.key.windowID);
        payload.type           = event_type::key_released;
        payload.keyboard.state = button_state::up;
        payload.keyboard.key   = static_cast<keycode>(event.key.scancode);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN :
        EQ_CHECK_WINDOW_ID(event.motion.windowID);
        payload.type = event_type::mouse_pressed;
        populate_mouse_event_payload(payload, event);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP :
        EQ_CHECK_WINDOW_ID(event.motion.windowID);
        payload.type = event_type::mouse_released;
        populate_mouse_event_payload(payload, event);
        break;
    case SDL_EVENT_MOUSE_MOTION :
        EQ_CHECK_WINDOW_ID(event.motion.windowID);
        payload.type = event_type::mouse_move;
        populate_mouse_event_payload(payload, event);
        break;
    case SDL_EVENT_MOUSE_WHEEL :
        EQ_CHECK_WINDOW_ID(event.wheel.windowID);
        payload.type         = event_type::mouse_scroll;
        payload.scroll.delta = {event.wheel.x, event.wheel.y};
        payload.scroll.pos   = {event.wheel.mouse_x, event.wheel.mouse_y};
        break;
    case SDL_EVENT_WINDOW_RESIZED :
        EQ_CHECK_WINDOW_ID(event.window.windowID);
        payload.type        = event_type::window_size_changed;
        payload.window.size = {event.window.data1, event.window.data2};
        break;
    case SDL_EVENT_WINDOW_EXPOSED :
        EQ_CHECK_WINDOW_ID(event.window.windowID);
        payload.type        = event_type::window_size_changed;
        payload.window.size = {event.window.data1, event.window.data2};
        send_event(payload);
        payload.type = event_type::request_draw;
        send_event(payload);
        return;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED :
        payload.type = event_type::request_close;
    default :
        break;
    }

    send_event(payload);
}

void events_queue::send_event(const event_payload& payload)
{
    if (payload.type != event_type::dummy && m_watchers.contains(payload.type))
    {
        std::invoke(m_watchers[payload.type], payload);
    }
}
