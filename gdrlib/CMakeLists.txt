cmake_minimum_required(VERSION 3.17)

project(gdrlib LANGUAGES C CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.c"
        "${PROJECT_SOURCE_DIR}/src/*.cc"
)

if (UNIX)
    enable_language(ASM)
    file(GLOB_RECURSE ASM_SOURCES "${PROJECT_SOURCE_DIR}/src/*.S")
    list(APPEND SOURCES ${ASM_SOURCES})
endif ()

#TODO: get rid of codegen, it was bad idea :(
include(../cmake/py_venv.cmake)
include(../cmake/codegen/codegen.cmake)
set(CODEGEN_ROOT "${CMAKE_BINARY_DIR}/.codegen")
run_codegen(ROOT "${CODEGEN_ROOT}" SOURCES ${SOURCES})

add_library(${PROJECT_NAME}
        STATIC
        ${SOURCES}
        ${CODEGEN_GENERATED_HEADERS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        shaders

        glm::glm
        EnTT::EnTT
        volk::volk
        quill::quill
        Tracy::TracyClient
        SDL3_image::SDL3_image
        SPIRV-Headers::SPIRV-Headers
        GPUOpen::VulkanMemoryAllocator

        imgui
        cgltf
        ddspp
        meshoptimizer
        nlohmann_json
)

if (APPLE)
    find_package(Vulkan REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
endif ()

target_compile_definitions(${PROJECT_NAME} PUBLIC
        TRACY_FIBERS
        VK_NO_PROTOTYPES
)

target_include_directories(${PROJECT_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
        "${CODEGEN_ROOT}"
)

add_dependencies(${PROJECT_NAME} run_codegen)
