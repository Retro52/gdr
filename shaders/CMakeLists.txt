project(shaders)

function(parse_shader_profile SHADER_FILE)
    get_filename_component(SHADER_EXTENSION "${SHADER_FILE}" LAST_EXT)

    if (SHADER_EXTENSION STREQUAL ".vert")
        set(SHADER_STAGE_LOCAL "vs")
    elseif (SHADER_EXTENSION STREQUAL ".frag")
        set(SHADER_STAGE_LOCAL "ps")
    elseif (SHADER_EXTENSION STREQUAL ".comp")
        set(SHADER_STAGE_LOCAL "cs")
    elseif (SHADER_EXTENSION STREQUAL ".mesh")
        set(SHADER_STAGE_LOCAL "ms")
    elseif (SHADER_EXTENSION STREQUAL ".task")
        set(SHADER_STAGE_LOCAL "as")
    else ()
        message(FATAL_ERROR "Unable to determine shader stage for '${SHADER_FILE}'")
    endif ()

    set(SHADER_PROFILE "${SHADER_STAGE_LOCAL}_6_8" PARENT_SCOPE)
endfunction()

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
    string(REGEX MATCH "^[^ ]+" SHADER_LOCAL "${SHADER_CONFIG}")

    string(REGEX MATCH "-o +([^ ]+)" _MATCH "${SHADER_CONFIG}")
    set(SHADER_BIN_LOCAL "${CMAKE_MATCH_1}")
    if (NOT SHADER_BIN_LOCAL)
        set(SHADER_BIN_LOCAL "${SHADER_LOCAL}")
    endif ()

    string(REGEX MATCH "-d +([^-|^\n]+)" _MATCH "${SHADER_CONFIG}")
    if (CMAKE_MATCH_1)
        string(STRIP "${CMAKE_MATCH_1}" CMAKE_MATCH_1)
        string(REPLACE " " ";-D" SHADER_DEF_LOCAL "-D${CMAKE_MATCH_1}")
    endif ()

    set(SHADER ${SHADER_LOCAL} PARENT_SCOPE)
    set(SHADER_BIN ${SHADER_BIN_LOCAL} PARENT_SCOPE)
    set(SHADER_DEF ${SHADER_DEF_LOCAL} PARENT_SCOPE)
endfunction()

find_package(Vulkan REQUIRED COMPONENTS glslc glslangValidator)

if (WIN32)
    find_program(SPIRV_CROSS_EXECUTABLE
            NAMES spirv-cross
            HINTS "$ENV{VULKAN_SDK}/bin"
            REQUIRED
    )

    find_program(DXC_EXECUTABLE
            NAMES dxc
            HINTS "$ENV{VULKAN_SDK}/bin"
            REQUIRED
    )
endif ()

file(READ "${PROJECT_SOURCE_DIR}/shaders.cfg" SHADERS_CONFIG)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/shaders.cfg")

string(STRIP "${SHADERS_CONFIG}" SHADERS_CONFIG)
string(REPLACE ";" "__SKIP__" SHADERS_CONFIG "${SHADERS_CONFIG}")
string(REPLACE "\n" ";" SHADERS_CONFIG "${SHADERS_CONFIG}")

set(SHADERS_BIN_FOLDER "${PROJECT_SOURCE_DIR}/bin")
set(SHADERS_BIN_FOLDER "${SHADERS_BIN_FOLDER}" PARENT_SCOPE)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SHADER_OPT_FLAGS -g)
else ()
    set(SHADER_OPT_FLAGS -Os)
endif ()

foreach (SHADER_CONFIG IN LISTS SHADERS_CONFIG)
    if (NOT SHADER_CONFIG)
        continue()
    endif ()

    string(FIND "${SHADER_CONFIG}" "__SKIP__" SC_COMMENTED_OUT)
    if ("${SC_COMMENTED_OUT}" EQUAL 0)
        continue()
    endif ()

    parse_shader_config("${SHADER_CONFIG}")
    parse_shader_profile("${SHADER}")
    collect_shader_dependencies("${SHADER}" SHADER_DEPENDENCIES)

    set(SHADER_INPUT_PATH "${PROJECT_SOURCE_DIR}/${SHADER}")
    set(SHADER_BIN_OUTPUT "${SHADERS_BIN_FOLDER}/${SHADER_BIN}")
    set(SHADER_HLSL_OUTPUT "${SHADERS_BIN_FOLDER}/__tmp_${SHADER_BIN}.hlsl")

    if (APPLE)
        list(APPEND SHADER_DEF "-DPLATFORM_MVK")
    endif ()

    set(SHADER_DXIL_COMMANDS "")
    add_custom_command(
            OUTPUT "${SHADER_BIN_OUTPUT}.spv"
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_BIN_FOLDER}
            COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} "${SHADER_INPUT_PATH}" -o "${SHADER_BIN_OUTPUT}.spv" --target-env vulkan1.3 --target-env spirv1.5 "$<IF:$<CONFIG:Debug>,-g,-Os>" ${SHADER_DEF}
            DEPENDS ${SHADER_DEPENDENCIES}
            COMMENT "Compiling shader '${SHADER}' to '${SHADER_BIN_OUTPUT}.spv'"
    )
    list(APPEND BIN_SHADERS "${SHADER_BIN_OUTPUT}.spv")

    if (WIN32 AND GDR_ENABLE_DX12_BACKEND)
        if (NOT DEFINED SPIRV_CROSS_EXECUTABLE OR NOT DEFINED DXC_EXECUTABLE)
            message(FATAL_ERROR "windows compiles dx12 backend which requires spirv-cross and dxc executables")
        endif ()
        add_custom_command(
                OUTPUT "${SHADER_BIN_OUTPUT}.dxl"
                COMMAND ${SPIRV_CROSS_EXECUTABLE} "${SHADER_BIN_OUTPUT}.spv" --hlsl --shader-model 68 --hlsl-enable-16bit-types --output "${SHADER_HLSL_OUTPUT}"
                COMMAND ${DXC_EXECUTABLE} -T ${SHADER_PROFILE} -E main -WX -Fo "${SHADER_BIN_OUTPUT}.dxl" "$<$<CONFIG:Debug>:-Zi>" "$<$<CONFIG:Debug>:-Qembed_debug>" "$<$<NOT:$<CONFIG:Debug>>:-O3>" "${SHADER_HLSL_OUTPUT}"
#                COMMAND ${CMAKE_COMMAND} -E rm -f "${SHADER_HLSL_OUTPUT}"
                DEPENDS "${SHADER_BIN_OUTPUT}.spv"
                COMMENT "Converting shader from '${SHADER_BIN_OUTPUT}.spv' to '${SHADER_BIN_OUTPUT}.dxl'"
        )

        list(APPEND BIN_SHADERS "${SHADER_BIN_OUTPUT}.dxl")
    endif ()
endforeach ()

set(BUILD_TARGET_NAME ${PROJECT_NAME}_build)
add_custom_target(${BUILD_TARGET_NAME} ALL DEPENDS ${BIN_SHADERS})

add_library(${PROJECT_NAME} INTERFACE)
add_dependencies(${PROJECT_NAME} ${BUILD_TARGET_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR}/include)
