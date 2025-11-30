#include <render/static_model.hpp>
#include <window.hpp>

window::window(std::string_view title, ivec2 size, bool fullscreen)
{
    SDL_Init(SDL_INIT_VIDEO);
    m_window = SDL_CreateWindow(title.data(),
                                size.x,
                                size.y,
                                (fullscreen ? SDL_WINDOW_FULLSCREEN : 0) | SDL_WINDOW_HIGH_PIXEL_DENSITY
                                    | SDL_WINDOW_RESIZABLE);

#if defined(SDL_PLATFORM_APPLE)
    m_metal_view = SDL_Metal_CreateView(m_window);
#endif
}

window::~window()
{
#if defined(SDL_PLATFORM_APPLE)
    SDL_Metal_DestroyView(m_metal_view);
#endif
    SDL_DestroyWindow(m_window);
}

[[nodiscard]] window::native window::get_native_handle() const noexcept
{
    native handle                = {.window = m_window};
    const SDL_PropertiesID props = SDL_GetWindowProperties(m_window);

#ifdef SDL_PLATFORM_WINDOWS
    handle.type         = driver_type::windows;
    handle.windows.hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(SDL_PLATFORM_LINUX)
    handle.wayland_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (handle.wayland_surface)
    {
        handle.type            = driver_type::wayland;
        handle.wayland_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    }
    else
    {
        // Fall back to X11
        handle.type        = driver_type::x11;
        handle.x11_window  = (void*)(uintptr_t)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        handle.x11_display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    }
#elif defined(SDL_PLATFORM_APPLE)
    handle.type             = driver_type::metal;
    handle.metal.metal_view = m_metal_view;
    handle.metal.ca_layer   = SDL_Metal_GetLayer(m_metal_view);
#endif

    return handle;
}

bool window::set_size(ivec2 size) const noexcept
{
    return SDL_SetWindowSize(m_window, size.x, size.y);
}

[[nodiscard]] ivec2 window::get_size() const noexcept
{
    ivec2 ret;
    SDL_GetWindowSize(m_window, &ret.x, &ret.y);

    return ret;
}

[[nodiscard]] ivec2 window::get_size_in_px() const noexcept
{
    ivec2 ret;
    SDL_GetWindowSizeInPixels(m_window, &ret.x, &ret.y);

    return ret;
}
