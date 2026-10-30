cmake_minimum_required(VERSION 3.17)

project(gdr LANGUAGES C CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF)
set(GDR_ENABLE_DX12_BACKEND ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(vendors)
add_subdirectory(shaders)

add_subdirectory(gdrlib)
add_subdirectory(demo)
