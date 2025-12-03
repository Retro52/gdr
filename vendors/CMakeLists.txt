add_subdirectory(glm)
add_subdirectory(vma)
add_subdirectory(sdl3)
add_subdirectory(entt)
add_subdirectory(json)
add_subdirectory(assimp)

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE AND NOT IOS)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_METAL_EXT)
elseif (LINUX)
    # https://stackoverflow.com/questions/45536141/how-i-can-find-out-if-a-linux-system-uses-wayland-or-x11/45537237#45537237
    if (DEFINED ENV{WAYLAND_DISPLAY})
        set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
    elseif (DEFINED ENV{DISPLAY})
        set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
    endif ()
else ()
    message(FATAL_ERROR "Unsupported vulkan platform")
endif ()

add_subdirectory(volk)

#set(NRI_ENABLE_IMGUI_EXTENSION ON)
#set(NRI_ENABLE_VALIDATION_SUPPORT ON)
#add_subdirectory(nri)

file(GLOB IMGUI_SOURCES "imgui/*.cpp" "imgui/*.c")
file(GLOB IMGUIZMO_SOURCES "imguizmo/*.cpp" "imguizmo/*.c")

list(APPEND IMGUI_SOURCES "imgui/misc/cpp/imgui_stdlib.cpp")
list(APPEND IMGUI_SOURCES "imgui/backends/imgui_impl_sdl3.cpp")
list(APPEND IMGUI_SOURCES "imgui/backends/imgui_impl_vulkan.cpp")

add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES})

target_include_directories(imgui PUBLIC imgui imguizmo)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

target_link_libraries(imgui PUBLIC SDL3::SDL3 volk::volk)
