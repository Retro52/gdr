project(gch_small_vector VERSION 0.10.4 LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)
add_library(gch::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR})
