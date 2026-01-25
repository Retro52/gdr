#pragma once

#include <SDL3/SDL.h>

#include <types.hpp>

#include <assert2.hpp>
#include <debug.hpp>

#include <string>

static SDL_Window* g_window = nullptr;

static std::string wrap_text(const char* text)
{
    constexpr u32 kMaxLines = 20;
    constexpr u32 kMaxWidth = 160;

    std::string result;
    std::string_view input(text);

    int line_count  = 0;
    u64 newline_pos = 0;
    while (!input.empty() && line_count < kMaxLines && newline_pos != std::string_view::npos)
    {
        newline_pos           = input.find('\n');
        std::string_view line = input.substr(0, std::min(newline_pos, input.size()));

        while (!line.empty() && line_count++ < kMaxLines)
        {
            if (line.size() <= kMaxWidth)
            {
                result += line;
                result += '\n';
                break;
            }

            size_t break_pos = line.rfind(' ', kMaxWidth);
            if (break_pos == std::string_view::npos || break_pos == 0)
            {
                break_pos = kMaxWidth;
            }

            result.append(line.substr(0, break_pos)).append("\n");
            line = line.substr(std::min(break_pos + 1, line.size()));

            while (!line.empty() && line[0] == ' ')
            {
                line = line.substr(1);
            }
        }

        input = input.substr(newline_pos + 1);
    }

    if (newline_pos != std::string_view::npos || line_count >= kMaxLines)
    {
        result += "... [truncated]";
    }

    if (!result.empty() && result.back() == '\n')
    {
        result.pop_back();
    }
    return result;
}

void debug::assert2_set_window(SDL_Window* window)
{
    g_window = window;
}

void debug::assert2_show_assert_popup(const char* message)
{
    enum button_ids : int
    {
        eSkip  = 1,
        eBreak = 2,
        eAbort = 3,
    };

    SDL_MessageBoxButtonData kNoDebuggerButtons[2] = {
        SDL_MessageBoxButtonData {.buttonID = eSkip,  .text = "Skip" },
        SDL_MessageBoxButtonData {.buttonID = eAbort, .text = "Abort"},
    };

    SDL_MessageBoxButtonData kDebuggerPresentButtons[3] = {
        SDL_MessageBoxButtonData {.buttonID = eSkip,  .text = "Skip" },
        SDL_MessageBoxButtonData {.buttonID = eBreak, .text = "Break"},
        SDL_MessageBoxButtonData {.buttonID = eAbort, .text = "Abort"},
    };

    const int buttons_count =
        debug::is_debugger_present() ? COUNT_OF(kDebuggerPresentButtons) : COUNT_OF(kNoDebuggerButtons);
    auto&& buttons = debug::is_debugger_present() ? kDebuggerPresentButtons : kNoDebuggerButtons;

    const auto msg = wrap_text(message);
    const SDL_MessageBoxData msg_box_data {
        .flags      = SDL_MESSAGEBOX_ERROR,
        .window     = g_window,
        .title      = "Soft Assert",
        .message    = msg.c_str(),
        .numbuttons = buttons_count,
        .buttons    = buttons,
    };

    int clicked = -1;
    SDL_ShowMessageBox(&msg_box_data, &clicked);

    switch (clicked)
    {
    case eBreak :
        debug::break_into_debugger();
        break;
    case eAbort :
        std::abort();
    default :
    case eSkip :
        break;
    }
}
