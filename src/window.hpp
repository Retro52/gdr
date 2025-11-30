#pragma once

#include <SDL3/SDL.h>
#if defined(SDL_PLATFORM_APPLE)
#include <SDL3/SDL_metal.h>
#endif

#include <types.hpp>

#include <string_view>

class window
{
public:
    enum class driver_type
    {
        windows,
        x11,
        wayland,
        metal,
        unknown
    };

    struct native
    {
        SDL_Window* window {nullptr};
        driver_type type {driver_type::unknown};

        union
        {
            struct dummy
            {
            } dummy {};

            struct windows
            {
                void* hwnd;  // Windows HWND
            } windows;

            struct metal
            {
                void* ca_layer;
                void* metal_view;
            } metal;

            struct wayland
            {
                void* surface;  // Wayland wl_surface*
                void* display;  // Wayland wl_display*
            } wayland;

            struct x11
            {
                void* window;   // X11 Window (cast to ::Window)
                void* display;  // X11 Display*
            } x11;
        };
    };

public:
    window(std::string_view title, ivec2 size, bool fullscreen);

    ~window();

    [[nodiscard]] native get_native_handle() const noexcept;

    bool set_size(ivec2 size) const noexcept;

    [[nodiscard]] ivec2 get_size() const noexcept;

    [[nodiscard]] ivec2 get_size_in_px() const noexcept;

private:
    SDL_Window* m_window = nullptr;

#if defined(SDL_PLATFORM_APPLE)
    SDL_MetalView m_metal_view = nullptr;
#endif
};
