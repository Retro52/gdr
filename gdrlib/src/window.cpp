#include <SDL3/SDL.h>

#include <tracy/Tracy.hpp>
#include <window.hpp>

window::window(const char* title, const create_window_info& create_info)
{
    ZoneScoped;

    SDL_Init(SDL_INIT_VIDEO);
    const auto props = SDL_CreateProperties();

    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);

    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, create_info.size.x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, create_info.size.y);

    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, create_info.position.x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, create_info.position.y);

    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, create_info.fullscreen);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, create_info.borderless);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, create_info.resizable);

    m_window = SDL_CreateWindowWithProperties(props);

    SDL_DestroyProperties(props);

#if defined(SDL_PLATFORM_APPLE)
    m_metal_view = SDL_Metal_CreateView(m_window);
#endif
}

window::~window()
{
    ZoneScoped;
#if defined(SDL_PLATFORM_APPLE)
    SDL_Metal_DestroyView(m_metal_view);
#endif
    SDL_DestroyWindow(m_window);
}

void window::set_fullscreen(bool fullscreen) noexcept
{
    SDL_SetWindowFullscreen(m_window, fullscreen);
}

[[nodiscard]] bool window::get_fullscreen() const noexcept
{
    return SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN;
}

[[nodiscard]] window::native window::get_native_handle() const noexcept
{
    ZoneScoped;

    native handle                = {.window = m_window};
    const SDL_PropertiesID props = SDL_GetWindowProperties(m_window);

#if defined(SDL_PLATFORM_WINDOWS)
    handle.type         = driver_type::windows;
    handle.windows.hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(SDL_PLATFORM_LINUX)
    handle.wayland.surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (handle.wayland.surface)
    {
        handle.type            = driver_type::wayland;
        handle.wayland.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    }
    else
    {
        // Fall back to X11
        handle.type       = driver_type::x11;
        handle.x11.window = reinterpret_cast<void*>(
            static_cast<u64>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
        handle.x11.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
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
    ZoneScoped;
    return SDL_SetWindowSize(m_window, size.x, size.y);
}

[[nodiscard]] ivec2 window::get_size() const noexcept
{
    ZoneScoped;

    ivec2 ret;
    SDL_GetWindowSize(m_window, &ret.x, &ret.y);

    return ret;
}

[[nodiscard]] ivec2 window::get_size_in_px() const noexcept
{
    ZoneScoped;

    ivec2 ret;
    SDL_GetWindowSizeInPixels(m_window, &ret.x, &ret.y);

    return ret;
}
