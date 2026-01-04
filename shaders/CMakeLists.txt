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


find_package(Vulkan REQUIRED COMPONENTS glslc glslangValidator)

file(GLOB_RECURSE SHADERS
        ${PROJECT_SOURCE_DIR}/*.vert
        ${PROJECT_SOURCE_DIR}/*.frag
        ${PROJECT_SOURCE_DIR}/*.comp
        ${PROJECT_SOURCE_DIR}/*.geom
        ${PROJECT_SOURCE_DIR}/*.tesc
        ${PROJECT_SOURCE_DIR}/*.tese
        ${PROJECT_SOURCE_DIR}/*.mesh
        ${PROJECT_SOURCE_DIR}/*.task
        ${PROJECT_SOURCE_DIR}/*.rgen
        ${PROJECT_SOURCE_DIR}/*.rchit
        ${PROJECT_SOURCE_DIR}/*.rmiss
)

foreach (SHADER IN LISTS SHADERS)
    get_filename_component(FILENAME "${SHADER}" NAME)
    get_filename_component(SHADER_DIRECTORY "${SHADER}" DIRECTORY)

    collect_shader_dependencies("${SHADER}" SHADER_DEPENDENCIES)

    message(STATUS "Dependencies of ${SHADER}: ${SHADER_DEPENDENCIES}")

    add_custom_command(
            OUTPUT "${SHADER_DIRECTORY}/${FILENAME}.spv"
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${SHADER}" -o "${SHADER_DIRECTORY}/${FILENAME}.spv" --target-env=vulkan1.3
            # COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} "${SHADER}" -o "${SHADER_DIRECTORY}/${FILENAME}.spv" --target-env vulkan1.3
            DEPENDS ${SHADER_DEPENDENCIES}
            COMMENT "Compiling ${FILENAME}"
    )
    list(APPEND SPV_SHADERS "${SHADER_DIRECTORY}/${FILENAME}.spv")
endforeach ()

set(BUILD_TARGET_NAME ${PROJECT_NAME}_build)
add_custom_target(${BUILD_TARGET_NAME} ALL DEPENDS ${SPV_SHADERS})

add_library(${PROJECT_NAME} INTERFACE)
add_dependencies(${PROJECT_NAME} ${BUILD_TARGET_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR}/include)
