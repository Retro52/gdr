#pragma once

struct SDL_Window;

namespace debug
{
    void assert2_set_window(SDL_Window* window);
    void assert2_show_assert_popup(const char* message);
}

#define assert2(EXPR)                              \
    if (!(EXPR))                                     \
    {                                              \
        ::debug::assert2_show_assert_popup(#EXPR); \
    }

#define assert2m(EXPR, MESSAGE)                      \
    if (!(EXPR))                                       \
    {                                                \
        ::debug::assert2_show_assert_popup(MESSAGE); \
    }
