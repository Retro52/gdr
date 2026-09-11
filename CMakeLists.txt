cmake_minimum_required(VERSION 3.17)

project(gdr LANGUAGES C CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(vendors)
add_subdirectory(shaders)

add_subdirectory(gdrlib)

add_subdirectory(demo)
add_subdirectory(rhi_test)

add_custom_target(clean_build
        COMMAND ${CMAKE_COMMAND} -E echo "Removing ${CMAKE_BINARY_DIR}/*.exe..."
        COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/*.exe"
        COMMAND ${CMAKE_COMMAND} -E echo "Removing ${SHADERS_BIN_FOLDER}..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SHADERS_BIN_FOLDER}
        COMMAND ${CMAKE_COMMAND} -E echo "Clean complete."
)
