cmake_minimum_required(VERSION 3.17)

project(dye LANGUAGES C CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(vendors)

find_package(Vulkan REQUIRED COMPONENTS glslc)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
file(GLOB_RECURSE SHADERS
        ${SHADER_DIR}/*.vert
        ${SHADER_DIR}/*.frag
        ${SHADER_DIR}/*.comp
        ${SHADER_DIR}/*.geom
        ${SHADER_DIR}/*.tesc
        ${SHADER_DIR}/*.tese
        ${SHADER_DIR}/*.mesh
        ${SHADER_DIR}/*.task
        ${SHADER_DIR}/*.rgen
        ${SHADER_DIR}/*.rchit
        ${SHADER_DIR}/*.rmiss
)

foreach (SHADER IN LISTS SHADERS)
    get_filename_component(FILENAME ${SHADER} NAME)
    get_filename_component(SHADER_DIRECTORY ${SHADER} DIRECTORY)
    add_custom_command(OUTPUT ${SHADER_DIRECTORY}/${FILENAME}.spv
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_DIRECTORY}/${FILENAME}.spv
            DEPENDS ${SHADER}
            COMMENT "Compiling ${FILENAME}")
    list(APPEND SPV_SHADERS ${SHADER_DIRECTORY}/${FILENAME}.spv)
endForeach ()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.c" "${CMAKE_SOURCE_DIR}/src/*.cc")

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${SHADERS})
add_dependencies(${CMAKE_PROJECT_NAME} shaders)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        glm::glm
        EnTT::EnTT
        volk::volk
        assimp::assimp
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator

        imgui
        nlohmann_json
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        VK_NO_PROTOTYPES
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
)
