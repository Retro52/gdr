add_subdirectory(glm)
add_subdirectory(gch)
add_subdirectory(vma)
add_subdirectory(sdl3)
add_subdirectory(entt)
add_subdirectory(json)
add_subdirectory(quill)
add_subdirectory(spirv-headers)
add_subdirectory(meshoptimizer)

if (WIN32)
    add_subdirectory(d3d12ma)
    add_subdirectory(dx-headers)
endif ()

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

if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    set(TRACY_ENABLE ON)
    set(TRACY_FIBERS ON)
    set(TRACY_ON_DEMAND ON)
else ()
    set(TRACY_ENABLE OFF)
endif ()
cmake_policy(SET CMP0077 NEW)
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
if (WIN32)
    list(APPEND IMGUI_SOURCES "imgui/backends/imgui_impl_dx12.cpp")
endif ()

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC imgui ImGuizmo/src)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

target_link_libraries(imgui PUBLIC SDL3::SDL3 volk::volk)

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE cgltf)

add_library(ddspp INTERFACE)
target_include_directories(ddspp INTERFACE ddspp)

add_subdirectory(tinyexr)
add_library(tinyexr::tinyexr ALIAS tinyexr)
target_include_directories(tinyexr PUBLIC "tinyexr")

add_library(ffx_api INTERFACE)
add_library(ffx::ffx_api ALIAS ffx_api)
target_include_directories(ffx_api
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/fidelity-fx-sdk/Kits/FidelityFX/api/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fidelity-fx-sdk/Kits/FidelityFX/upscalers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fidelity-fx-sdk/Kits/FidelityFX/framegeneration/include
)

function(add_ffx_dependencies)
    cmake_parse_arguments(FFX "" "TARGET" "COMPONENTS;BACKENDS" ${ARGN})
    if (NOT WIN32)
        return()
    endif ()

    foreach (FFX_BACKEND IN LISTS FFX_BACKENDS)
        string(TOUPPER "${FFX_BACKEND}" FFX_BACKEND)
        if (FFX_BACKEND STREQUAL "VULKAN")
            list(APPEND FFX_INTERNAL_POSTFIXES "vk")
        elseif (FFX_BACKEND STREQUAL "D3D12")
            list(APPEND FFX_INTERNAL_POSTFIXES "dx12")
        endif ()
    endforeach ()

    get_target_property(FFX_API_SOURCE_DIR ffx::ffx_api SOURCE_DIR)
    set(FFX_API_ROOT_BIN "${FFX_API_SOURCE_DIR}/fidelity-fx-sdk/Kits/FidelityFX/signedbin")

    foreach (FFX_POSTFIX IN LISTS FFX_INTERNAL_POSTFIXES)
        target_link_libraries(${FFX_TARGET}
                PRIVATE
                ffx::ffx_api)
        add_custom_command(TARGET ${FFX_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FFX_API_ROOT_BIN}/amd_fidelityfx_loader_${FFX_POSTFIX}.dll $<TARGET_FILE_DIR:${FFX_TARGET}>)

        foreach (FFX_COMPONENT IN LISTS FFX_COMPONENTS)
            string(TOUPPER "${FFX_COMPONENT}" FFX_COMPONENT)
            if (FFX_COMPONENT STREQUAL "UPSCALE")
                add_custom_command(TARGET ${FFX_TARGET} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${FFX_API_ROOT_BIN}/amd_fidelityfx_upscaler_${FFX_POSTFIX}.dll $<TARGET_FILE_DIR:${FFX_TARGET}>)
            elseif (FFX_COMPONENT STREQUAL "FRAMEGEN")
                add_custom_command(TARGET ${FFX_TARGET} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${FFX_API_ROOT_BIN}/amd_fidelityfx_framegeneration_${FFX_POSTFIX}.dll $<TARGET_FILE_DIR:${FFX_TARGET}>)
            else ()
                message(WARNING "Unknown VK FFX component ${FFX_COMPONENT}")
            endif ()
        endforeach ()
    endforeach ()
endfunction()

add_library(pix3_api INTERFACE)
add_library(pix3::pix3_api ALIAS pix3_api)
target_include_directories(pix3_api
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/win-pix-runtime/Include
)

function(add_pix3_dependency PIX3_TARGET)
    if (NOT WIN32)
        return()
    endif ()

    get_target_property(PIX3_API_SOURCE_DIR pix3::pix3_api SOURCE_DIR)
    set(PIX3_API_ROOT_BIN "${PIX3_API_SOURCE_DIR}/win-pix-runtime/bin/x64/")

    target_link_libraries(${PIX3_TARGET}
            PRIVATE
            pix3::pix3_api
            "${PIX3_API_ROOT_BIN}/WinPixEventRuntime.lib"
    )
    add_custom_command(TARGET ${PIX3_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PIX3_API_ROOT_BIN}/WinPixEventRuntime.dll" $<TARGET_FILE_DIR:${PIX3_TARGET}>
    )
endfunction()
