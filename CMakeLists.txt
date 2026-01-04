cmake_minimum_required(VERSION 3.17)

project(gdr LANGUAGES C CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(vendors)
add_subdirectory(shaders)

file(GLOB_RECURSE SOURCES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_SOURCE_DIR}/src/*.c"
        "${CMAKE_SOURCE_DIR}/src/*.cc"
)

if (UNIX)
    enable_language(ASM)
    file(GLOB_RECURSE ASM_SOURCES "${CMAKE_SOURCE_DIR}/src/*.S")
    list(APPEND SOURCES ${ASM_SOURCES})
endif ()

include(cmake/codegen/codegen.cmake)
set(CODEGEN_ROOT "${CMAKE_BINARY_DIR}/.codegen")
run_codegen(ROOT "${CODEGEN_ROOT}" SOURCES ${SOURCES})

add_executable(${PROJECT_NAME} ${SOURCES} ${SHADERS} ${CODEGEN_GENERATED_HEADERS})
add_dependencies(${PROJECT_NAME} shaders)

target_link_libraries(${PROJECT_NAME} PRIVATE
        shaders

        glm::glm
        EnTT::EnTT
        volk::volk
        assimp::assimp
        Tracy::TracyClient
        GPUOpen::VulkanMemoryAllocator

        imgui
        meshoptimizer
        nlohmann_json
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_NO_PROTOTYPES
)

target_include_directories(${PROJECT_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
        "${CODEGEN_ROOT}"
)
