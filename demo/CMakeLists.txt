cmake_minimum_required(VERSION 3.17)

project(gdr LANGUAGES C CXX VERSION 1.0)

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

target_link_libraries(${PROJECT_NAME} PRIVATE
        shaders
        gdrlib
)

target_include_directories(${PROJECT_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
)
