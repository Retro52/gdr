project(shaders)

function(collect_shader_deps_recursive FILE_PATH)
    get_filename_component(ABS_PATH "${FILE_PATH}" ABSOLUTE)

    get_property(VISITED GLOBAL PROPERTY _SHADER_DEPS_VISITED)
    list(FIND VISITED "${ABS_PATH}" IDX)
    if (NOT IDX EQUAL -1)
        return()
    endif ()

    set_property(GLOBAL APPEND PROPERTY _SHADER_DEPS_VISITED "${ABS_PATH}")
    set_property(GLOBAL APPEND PROPERTY _SHADER_DEPS_ACCUMULATOR "${ABS_PATH}")

    if (EXISTS "${ABS_PATH}")
        file(READ "${ABS_PATH}" CONTENT)
        get_filename_component(FILE_DIRECTORY "${ABS_PATH}" DIRECTORY)

        string(REGEX MATCHALL "#include[ \t]*[<\"][^>\"]+[>\"]" INCLUDES "${CONTENT}")

        foreach (INCLUDE IN LISTS INCLUDES)
            string(REGEX REPLACE "#include[ \t]*[<\"]([^>\"]+)[>\"]" "\\1" HEADER_PATH "${INCLUDE}")
            set(FULL_HEADER_PATH "${FILE_DIRECTORY}/${HEADER_PATH}")

            if (EXISTS "${FULL_HEADER_PATH}")
                collect_shader_deps_recursive("${FULL_HEADER_PATH}")
            endif ()
        endforeach ()
    endif ()
endfunction()

function(collect_shader_dependencies SHADER_FILE OUT_DEPENDENCIES)
    set_property(GLOBAL PROPERTY _SHADER_DEPS_ACCUMULATOR "")
    set_property(GLOBAL PROPERTY _SHADER_DEPS_VISITED "")

    collect_shader_deps_recursive("${SHADER_FILE}")

    get_property(RESULT GLOBAL PROPERTY _SHADER_DEPS_ACCUMULATOR)
    set(${OUT_DEPENDENCIES} ${RESULT} PARENT_SCOPE)
endfunction()

function(parse_shader_config SHADER_CONFIG)
    string(REGEX MATCH "^[^ ]+" SHADER "${SHADER_CONFIG}")

    string(REGEX MATCH "-o +([^ ]+)" _MATCH "${SHADER_CONFIG}")
    set(SHADER_BIN "${CMAKE_MATCH_1}")
    if(NOT SHADER_BIN)
        set(SHADER_BIN "${SHADER}.spv")
    endif()

    string(REGEX MATCH "-d +([^-|^\n]+)" _MATCH "${SHADER_CONFIG}")
    if(CMAKE_MATCH_1)
        string(STRIP "${CMAKE_MATCH_1}" CMAKE_MATCH_1)
        string(REPLACE " " ";-D" SHADER_DEF "-D${CMAKE_MATCH_1}")
    endif()

    set(SHADER     ${SHADER}     PARENT_SCOPE)
    set(SHADER_BIN ${SHADER_BIN} PARENT_SCOPE)
    set(SHADER_DEF ${SHADER_DEF} PARENT_SCOPE)
endfunction()

find_package(Vulkan REQUIRED COMPONENTS glslc glslangValidator)

file(READ "${PROJECT_SOURCE_DIR}/shaders.cfg" SHADERS_CONFIG)

string(STRIP "${SHADERS_CONFIG}" SHADERS_CONFIG)
string(REPLACE "\n" ";" SHADERS_CONFIG "${SHADERS_CONFIG}")

foreach (SHADER_CONFIG IN LISTS SHADERS_CONFIG)
    parse_shader_config("${SHADER_CONFIG}")
    collect_shader_dependencies("${SHADER}" SHADER_DEPENDENCIES)

    set(SHADER_BIN_FOLDER "${PROJECT_SOURCE_DIR}/bin")
    set(SHADER_INPUT_PATH "${PROJECT_SOURCE_DIR}/${SHADER}")
    set(SHADER_BIN_OUTPUT "${SHADER_BIN_FOLDER}/${SHADER_BIN}")

    add_custom_command(
            OUTPUT "${SHADER_BIN_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_FOLDER}
            COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} "${SHADER_INPUT_PATH}" -o "${SHADER_BIN_OUTPUT}" --target-env vulkan1.3 ${SHADER_DEF}
            DEPENDS ${SHADER_DEPENDENCIES}
            COMMENT "Compiling shader '${SHADER}' to '${SHADER_BIN_OUTPUT}'"
    )
    list(APPEND SPV_SHADERS "${SHADER_BIN_OUTPUT}")
endforeach ()

set(BUILD_TARGET_NAME ${PROJECT_NAME}_build)
add_custom_target(${BUILD_TARGET_NAME} ALL DEPENDS ${SPV_SHADERS})

add_library(${PROJECT_NAME} INTERFACE)
add_dependencies(${PROJECT_NAME} ${BUILD_TARGET_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR}/include)
