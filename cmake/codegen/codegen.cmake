include(cmake/py_venv.cmake)

function(run_codegen)
    cmake_parse_arguments(RC_ARGS "" "ROOT" "SOURCES" "${ARGN}")

    message(STATUS "codegen: creating venv...")
    create_venv(PATH "${CMAKE_BINARY_DIR}/.venv" DEPENDENCIES tree-sitter tree-sitter-cpp Jinja2)

    if (DEFINED CLANG_FORMAT_EXECUTABLE AND EXISTS ${CLANG_FORMAT_EXECUTABLE})
        set(CLANG_FORMAT_FOUND TRUE)
        message(STATUS "codegen: detected clang-format at ${CLANG_FORMAT_EXECUTABLE}")
    else ()
        message(STATUS "codegen: clang-format not found")
    endif ()

    set(CODEGEN_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/codegen/codegen.py")
    set(CODEGEN_CONFIG "${CMAKE_SOURCE_DIR}/cmake/codegen/codegen.cfg.xml")

    list(FILTER RC_ARGS_SOURCES INCLUDE REGEX ".*\\.hpp$")
    set(CODEGEN_OUTPUTS "")

    file(MAKE_DIRECTORY "${RC_ARGS_ROOT}/codegen")
    file(COPY_FILE "${CMAKE_SOURCE_DIR}/cmake/codegen/common.hpp" "${RC_ARGS_ROOT}/codegen/common.hpp")

    foreach (SOURCE ${RC_ARGS_SOURCES})
        string(REPLACE "${CMAKE_SOURCE_DIR}" "" SOURCE_REL "${SOURCE}")
        string(REGEX REPLACE "^[\/|\\]src[\/|\\]" "" SOURCE_REL "${SOURCE_REL}")

        cmake_path(GET SOURCE_REL STEM SOURCE_STEM)
        cmake_path(GET SOURCE_REL PARENT_PATH SOURCE_PARENT)

        set(SOURCE_CODEGEN_OUTPUT "${RC_ARGS_ROOT}/codegen/${SOURCE_PARENT}/${SOURCE_STEM}.hpp")

        set(CODEGEN_ARGS
                ${CODEGEN_SCRIPT} ${SOURCE}
                -c ${CODEGEN_CONFIG}
                -o ${SOURCE_CODEGEN_OUTPUT}
                --include "<${SOURCE_REL}>"
                --include "<codegen/common.hpp>"
                --namespace detail_${SOURCE_STEM}
                --type-list ${SOURCE_STEM}
                --format
        )

        if (CLANG_FORMAT_FOUND)
            list(APPEND CODEGEN_ARGS --format --formatter ${CLANG_FORMAT_EXECUTABLE})
        endif ()

        # Create output directory at configure time
        add_custom_command(
                OUTPUT ${SOURCE_CODEGEN_OUTPUT}
                COMMAND ${VENV_Python3_EXECUTABLE} ${CODEGEN_ARGS}
                DEPENDS ${SOURCE} ${CODEGEN_SCRIPT} ${CODEGEN_CONFIG}
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/codegen"
                COMMENT "codegen: ${SOURCE} -> ${SOURCE_CODEGEN_OUTPUT}"
                VERBATIM
        )

        list(APPEND CODEGEN_OUTPUTS ${SOURCE_CODEGEN_OUTPUT})
    endforeach ()

    set(CODEGEN_GENERATED_HEADERS ${CODEGEN_OUTPUTS} PARENT_SCOPE)
    add_custom_target(run_codegen ALL DEPENDS ${CODEGEN_OUTPUTS})
endfunction()
