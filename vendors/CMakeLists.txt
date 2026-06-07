add_subdirectory(glm)
add_subdirectory(vma)
add_subdirectory(sdl3)
add_subdirectory(entt)
add_subdirectory(json)
add_subdirectory(quill)
add_subdirectory(spirv-headers)
add_subdirectory(meshoptimizer)

set(SDLIMAGE_XV OFF)
set(SDLIMAGE_ANI OFF)
set(SDLIMAGE_GIF OFF)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_LBM OFF)
set(SDLIMAGE_PCX OFF)
set(SDLIMAGE_PNM OFF)
set(SDLIMAGE_QOI OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_XCF OFF)
set(SDLIMAGE_XPM OFF)
set(SDLIMAGE_AVIF OFF)
add_subdirectory(sdl_image)

set(TRACY_ENABLE ON)
set(TRACY_FIBERS ON)
set(TRACY_ON_DEMAND ON)
add_subdirectory(tracy)

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE AND NOT IOS)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_METAL_EXT)
elseif (LINUX)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
else ()
    message(FATAL_ERROR "Unsupported vulkan platform")
endif ()

add_subdirectory(volk)

file(GLOB IMGUI_SOURCES "imgui/*.cpp" "imgui/*.c" "ImGuizmo/src/*")

list(APPEND IMGUI_SOURCES "imgui/misc/cpp/imgui_stdlib.cpp")
list(APPEND IMGUI_SOURCES "imgui/backends/imgui_impl_sdl3.cpp")
list(APPEND IMGUI_SOURCES "imgui/backends/imgui_impl_vulkan.cpp")

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC imgui ImGuizmo/src)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

target_link_libraries(imgui PUBLIC SDL3::SDL3 volk::volk)

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE cgltf)

add_library(ddspp INTERFACE)
target_include_directories(ddspp INTERFACE ddspp)
