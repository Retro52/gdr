cmake_minimum_required(VERSION 3.17)

project(gdr LANGUAGES C CXX VERSION 1.0)

macro(option2 NAME DEFAULT)
    if (NOT DEFINED ${NAME})
        set(${NAME} ${DEFAULT})
    endif ()
endmacro()

option2(GDR_LIVE_SHADER_RELOAD OFF)
option2(GDR_BIN_FOLDER "${CMAKE_SOURCE_DIR}/bin-${CMAKE_BUILD_TYPE}")

include(cmake/utils.cmake)
set_bin_dst(${GDR_BIN_FOLDER})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.c"
        "${PROJECT_SOURCE_DIR}/src/*.cc"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${SHADERS} ${CODEGEN_GENERATED_HEADERS})
add_dependencies(${PROJECT_NAME} shaders)

add_ffx_dependencies(
        TARGET ${PROJECT_NAME}
        BACKENDS VULKAN D3D12
        COMPONENTS UPSCALE
)

add_pix3_dependency(${PROJECT_NAME})
add_shaders_dependency(${PROJECT_NAME} "${GDR_BIN_FOLDER}/shaders")

target_link_libraries(${PROJECT_NAME} PRIVATE
        gdrlib
)

target_include_directories(${PROJECT_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
)

if (GDR_LIVE_SHADER_RELOAD)
    message(STATUS "Adding live shader reload macros")
    target_compile_definitions(${PROJECT_NAME}
            PRIVATE
            GDR_CMAKE_COMMAND="${CMAKE_COMMAND}"
            GDR_CMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
            GDR_SHADERS_TARGET_NAME="${PROJECT_NAME}_shaders_build"
    )
endif ()
