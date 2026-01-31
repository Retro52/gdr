#pragma once

#include <SDL3/SDL.h>

#include <types.hpp>

#include <window.hpp>

#include <array>
#include <unordered_map>

enum class event_type
{
    dummy,

    window_shown,
    window_hidden,
    window_size_changed,

    mouse_move,
    mouse_scroll,
    mouse_pressed,
    mouse_released,

    key_pressed,
    key_released,

    request_draw,
    request_close,

    count
};

enum class mouse_button
{
    left   = SDL_BUTTON_LEFT,
    right  = SDL_BUTTON_RIGHT,
    scroll = SDL_BUTTON_MIDDLE,
    x1     = SDL_BUTTON_X1,
    x2     = SDL_BUTTON_X1,
};

enum class button_state
{
    up,           // just raised
    down,         // just pressed
    double_down,  // aka pressed twice
    unknown
};

enum class keycode
{
    unknown,

    a = SDL_SCANCODE_A,
    b = SDL_SCANCODE_B,
    c = SDL_SCANCODE_C,
    d = SDL_SCANCODE_D,
    e = SDL_SCANCODE_E,
    f = SDL_SCANCODE_F,
    g = SDL_SCANCODE_G,
    h = SDL_SCANCODE_H,
    i = SDL_SCANCODE_I,
    j = SDL_SCANCODE_J,
    k = SDL_SCANCODE_K,
    l = SDL_SCANCODE_L,
    m = SDL_SCANCODE_M,
    n = SDL_SCANCODE_N,
    o = SDL_SCANCODE_O,
    p = SDL_SCANCODE_P,
    q = SDL_SCANCODE_Q,
    r = SDL_SCANCODE_R,
    s = SDL_SCANCODE_S,
    t = SDL_SCANCODE_T,
    u = SDL_SCANCODE_U,
    v = SDL_SCANCODE_V,
    w = SDL_SCANCODE_W,
    x = SDL_SCANCODE_X,
    y = SDL_SCANCODE_Y,
    z = SDL_SCANCODE_Z,

    num_1 = SDL_SCANCODE_1,
    num_2 = SDL_SCANCODE_2,
    num_3 = SDL_SCANCODE_3,
    num_4 = SDL_SCANCODE_4,
    num_5 = SDL_SCANCODE_5,
    num_6 = SDL_SCANCODE_6,
    num_7 = SDL_SCANCODE_7,
    num_8 = SDL_SCANCODE_8,
    num_9 = SDL_SCANCODE_9,
    num_0 = SDL_SCANCODE_0,

    kp_1 = SDL_SCANCODE_KP_1,
    kp_2 = SDL_SCANCODE_KP_2,
    kp_3 = SDL_SCANCODE_KP_3,
    kp_4 = SDL_SCANCODE_KP_4,
    kp_5 = SDL_SCANCODE_KP_5,
    kp_6 = SDL_SCANCODE_KP_6,
    kp_7 = SDL_SCANCODE_KP_7,
    kp_8 = SDL_SCANCODE_KP_8,
    kp_9 = SDL_SCANCODE_KP_9,
    kp_0 = SDL_SCANCODE_KP_0,

    f1  = SDL_SCANCODE_F1,
    f2  = SDL_SCANCODE_F2,
    f3  = SDL_SCANCODE_F3,
    f4  = SDL_SCANCODE_F4,
    f5  = SDL_SCANCODE_F5,
    f6  = SDL_SCANCODE_F6,
    f7  = SDL_SCANCODE_F7,
    f8  = SDL_SCANCODE_F8,
    f9  = SDL_SCANCODE_F9,
    f10 = SDL_SCANCODE_F10,
    f11 = SDL_SCANCODE_F11,
    f12 = SDL_SCANCODE_F12,

    sc_up    = SDL_SCANCODE_UP,
    sc_left  = SDL_SCANCODE_LEFT,
    sc_down  = SDL_SCANCODE_DOWN,
    sc_right = SDL_SCANCODE_RIGHT,

    sc_return    = SDL_SCANCODE_RETURN,
    sc_escape    = SDL_SCANCODE_ESCAPE,
    sc_backspace = SDL_SCANCODE_BACKSPACE,
    sc_tab       = SDL_SCANCODE_TAB,
    sc_space     = SDL_SCANCODE_SPACE,
    sc_ctrl      = SDL_SCANCODE_LCTRL,
    sc_shift     = SDL_SCANCODE_LSHIFT,
    sc_ctrl_r    = SDL_SCANCODE_RCTRL,
    sc_shift_r   = SDL_SCANCODE_RSHIFT,
};

struct dummy_event
{
    // just a placeholder
};

struct window_event
{
    ivec2 size;     // if window size was changed represents new window size
    ivec2 size_px;  // if window size in pixels was changed represents new window size in pixels
};

struct mouse_event
{
    vec2 pos;             // mouse pos
    vec2 delta;           // if mouse was moved represent the travel distance
    mouse_button button;  // if mouse button was pressed represents mouse button pressed
};

struct scroll_event
{
    vec2 pos;    // where mouse was when scrolling occurred
    vec2 delta;  // how much was scrolled in either direction
};

struct keyboard_event
{
    keycode key;
    button_state state;
};

struct event_payload
{
    event_type type {event_type::dummy};

    union
    {
        dummy_event dummy {};
        mouse_event mouse;
        window_event window;
        scroll_event scroll;
        keyboard_event keyboard;
    };
};

class events_queue
{
public:
    using watcher_t = void (*)(const event_payload& payload, void* user_data);

    struct watcher_data
    {
        watcher_t func  = nullptr;
        void* user_data = nullptr;
    };

    using watchers_array_t = std::list<watcher_data>;

public:
    explicit events_queue(const window& window);

    ~events_queue();

    void poll();

    void remove_watcher(void* user_data);

    void remove_watcher(event_type event, void* user_data);

    void remove_watcher(event_type event, const watcher_t& func);

    void add_watcher(event_type event, const watcher_t& func, void* user_data);

    [[nodiscard]] bool key_up(keycode key) const;

    [[nodiscard]] bool key_down(keycode key) const;

    [[nodiscard]] button_state get_key_state(keycode key) const;

    [[nodiscard]] button_state get_mouse_button_state(mouse_button button) const;

    void send_event(SDL_Event& event);

    void queue_event(SDL_Event& event) const;

    void hide_cursor() const;

    void show_cursor() const;

    void center_cursor() const;

private:
    void send_event(const event_payload& payload);

private:
    std::array<watchers_array_t, static_cast<u32>(event_type::count)> m_watchers;

    u32 m_window_id {0};
    u32 m_mouse_clicks_magic {0};
};
