#include <SDL3/SDL.h>

#include <backends/imgui_impl_sdl3.h>
#include <events.hpp>
#include <imgui_internal.h>
#include <Tracy/Tracy.hpp>

#include <iostream>
#include <ostream>

#define BREAK_IF(COND) \
    if ((COND))        \
        break;

#define EQ_CHECK_WINDOW_ID(ID) BREAK_IF(ID != m_window_id)

namespace
{
    ivec2 get_window_size_px(SDL_WindowID window_id)
    {
        ZoneScoped;
        ivec2 size_px;
        SDL_GetWindowSizeInPixels(SDL_GetWindowFromID(window_id), &size_px.x, &size_px.y);

        return size_px;
    }

    u16 get_mouse_clicks(mouse_button button, u32 buffer)
    {
        ZoneScoped;
        const u32 click_button = buffer >> 16;
        const u16 click_count  = static_cast<u16>(buffer & 0xFFFF);

        return click_count * (click_button == static_cast<u32>(button));
    }

    void write_mouse_clicks(mouse_button button, u16 count, u32& buffer)
    {
        ZoneScoped;
        buffer = (static_cast<u32>(button) << 16) | static_cast<u32>(count);
    }

    void populate_mouse_event_payload(event_payload& payload, SDL_Event& event)
    {
        ZoneScoped;
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

    bool window_resized_filter(void* userdata, SDL_Event* event)
    {
        ZoneScoped;
        if (event && event->type == SDL_EVENT_WINDOW_EXPOSED)
        {
            static_cast<events_queue*>(userdata)->send_event(*event);
        }

        return true;
    }

    bool imgui_captures_mouse()
    {
        return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
    }

    bool imgui_captures_keyboard()
    {
        return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
    }
}

events_queue::events_queue(const window& window)
    : m_window_id(SDL_GetWindowID(window.get_native_handle().window))
{
    ZoneScoped;
    SDL_InitSubSystem(SDL_INIT_EVENTS);
    SDL_AddEventWatch(window_resized_filter, this);
}

events_queue::~events_queue()
{
    ZoneScoped;
    SDL_RemoveEventWatch(window_resized_filter, this);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

void events_queue::poll()
{
    ZoneScoped;
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        send_event(event);
    }

    constexpr event_payload draw_event {.type = event_type::request_draw};
    send_event(draw_event);
}

void events_queue::add_watcher(event_type event, const watcher_t& func, void* user_data)
{
    ZoneScoped;
    m_watchers[static_cast<u32>(event)].emplace_back(func, user_data);
}

void events_queue::remove_watcher(event_type event, const watcher_t& func)
{
    ZoneScoped;
    auto& group = m_watchers[static_cast<u32>(event)];
    for (auto it = group.begin(); it != group.end(); ++it)
    {
        if (it->func == func)
        {
            group.erase(it);
            break;
        }
    }
}

bool events_queue::key_up(keycode key) const
{
    ZoneScoped;
    return get_key_state(key) == button_state::up;
}

bool events_queue::key_down(keycode key) const
{
    ZoneScoped;
    return get_key_state(key) == button_state::down;
}

button_state events_queue::get_key_state(keycode key) const
{
    ZoneScoped;
    if (SDL_GetWindowID(SDL_GetMouseFocus()) != m_window_id || imgui_captures_keyboard())
    {
        return button_state::unknown;
    }

    bool pressed = SDL_GetKeyboardState(nullptr)[static_cast<u32>(key)];
    return pressed ? button_state::down : button_state::up;
}

button_state events_queue::get_mouse_button_state(mouse_button button) const
{
    ZoneScoped;
    if (SDL_GetWindowID(SDL_GetMouseFocus()) != m_window_id || imgui_captures_mouse())
    {
        return button_state::unknown;
    }

    const bool pressed = SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(static_cast<u32>(button));
    const u16 clicks   = get_mouse_clicks(button, m_mouse_clicks_magic);

    return pressed ? (clicks > 1 ? button_state::double_down : button_state::down) : button_state::up;
}

void events_queue::send_event(SDL_Event& event)
{
    ZoneScoped;
    static event_payload payload;
    payload.type = event_type::dummy;

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN :
        BREAK_IF(imgui_captures_keyboard());
        EQ_CHECK_WINDOW_ID(event.key.windowID);
        payload.type           = event_type::key_pressed;
        payload.keyboard.state = button_state::down;
        payload.keyboard.key   = static_cast<keycode>(event.key.scancode);
        break;
    case SDL_EVENT_KEY_UP :
        BREAK_IF(imgui_captures_keyboard());
        EQ_CHECK_WINDOW_ID(event.key.windowID);
        payload.type           = event_type::key_released;
        payload.keyboard.state = button_state::up;
        payload.keyboard.key   = static_cast<keycode>(event.key.scancode);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN :
        BREAK_IF(imgui_captures_mouse());
        EQ_CHECK_WINDOW_ID(event.motion.windowID);
        payload.type = event_type::mouse_pressed;
        populate_mouse_event_payload(payload, event);
        write_mouse_clicks(payload.mouse.button, event.button.clicks, m_mouse_clicks_magic);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP :
        BREAK_IF(imgui_captures_mouse());
        EQ_CHECK_WINDOW_ID(event.motion.windowID);
        payload.type = event_type::mouse_released;
        populate_mouse_event_payload(payload, event);
        break;
    case SDL_EVENT_MOUSE_MOTION :
        BREAK_IF(imgui_captures_mouse());
        EQ_CHECK_WINDOW_ID(event.motion.windowID);
        payload.type = event_type::mouse_move;
        populate_mouse_event_payload(payload, event);
        break;
    case SDL_EVENT_MOUSE_WHEEL :
        BREAK_IF(imgui_captures_mouse());
        EQ_CHECK_WINDOW_ID(event.wheel.windowID);
        payload.type         = event_type::mouse_scroll;
        payload.scroll.delta = {event.wheel.x, event.wheel.y};
        payload.scroll.pos   = {event.wheel.mouse_x, event.wheel.mouse_y};
        break;
    case SDL_EVENT_WINDOW_RESIZED :
        EQ_CHECK_WINDOW_ID(event.window.windowID);
        payload.type           = event_type::window_size_changed;
        payload.window.size    = {event.window.data1, event.window.data2};
        payload.window.size_px = get_window_size_px(m_window_id);
        break;
    case SDL_EVENT_WINDOW_EXPOSED :
        EQ_CHECK_WINDOW_ID(event.window.windowID);
        payload.type           = event_type::window_size_changed;
        payload.window.size    = {event.window.data1, event.window.data2};
        payload.window.size_px = get_window_size_px(m_window_id);
        send_event(payload);
        payload.type = event_type::request_draw;
        send_event(payload);
        return;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED :
        payload.type = event_type::request_close;
        break;
    default :
        break;
    }

    send_event(payload);
}

void events_queue::queue_event(SDL_Event& event) const
{
    ZoneScoped;
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN :
    case SDL_EVENT_KEY_UP :
        event.key.windowID = m_window_id;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP :
    case SDL_EVENT_MOUSE_BUTTON_DOWN :
        event.button.windowID = m_window_id;
        break;
    case SDL_EVENT_MOUSE_MOTION :
        event.motion.windowID = m_window_id;
        break;
    case SDL_EVENT_WINDOW_RESIZED :
    case SDL_EVENT_WINDOW_EXPOSED :
        event.window.windowID = m_window_id;
        break;
    default :
        break;
    }
    SDL_PushEvent(&event);
}

void events_queue::hide_cursor() const
{
    SDL_HideCursor();
    if (auto* ctx = ImGui::GetCurrentContext())
    {
        ctx->IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
}

void events_queue::show_cursor() const
{
    SDL_HideCursor();
    if (auto* ctx = ImGui::GetCurrentContext())
    {
        ctx->IO.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }
}

void events_queue::center_cursor() const
{
    ivec2 window_size;
    auto* window = SDL_GetWindowFromID(m_window_id);
    SDL_GetWindowSize(window, &window_size.x, &window_size.y);

    SDL_WarpMouseInWindow(window, static_cast<f32>(window_size.x) / 2.0F, static_cast<f32>(window_size.y) / 2.0F);
}

void events_queue::send_event(const event_payload& payload)
{
    ZoneScoped;
    if (payload.type != event_type::dummy)
    {
        for (const auto& watcher : m_watchers[static_cast<u32>(payload.type)])
        {
            std::invoke(watcher.func, payload, watcher.user_data);
        }
    }
}
