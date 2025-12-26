include(cmake/py_venv.cmake)

function(run_codegen)
    cmake_parse_arguments(RC_ARGS "" "ROOT" "SOURCES" "${ARGN}")

    message(STATUS "codegen: creating venv...")
    create_venv(PATH "${CMAKE_BINARY_DIR}/.venv" DEPENDENCIES tree-sitter tree-sitter-cpp Jinja2)

#    find_package(ClangFormat)
    if (CLANG_FORMAT_FOUND)
        message(STATUS "codegen: clang-format v${CLANG_FORMAT_VERSION} found at ${CLANG_FORMAT_EXECUTABLE}")
    elseif (DEFINED CLANG_FORMAT_EXECUTABLE AND EXISTS ${CLANG_FORMAT_EXECUTABLE})
        set(CLANG_FORMAT_FOUND TRUE)
        message(STATUS "codegen: detected (user-specified?) clang-format at ${CLANG_FORMAT_EXECUTABLE}")
    else ()
        message(STATUS "codegen: clang-format not found")
    endif ()

    list(FILTER RC_ARGS_SOURCES INCLUDE REGEX ".*\\.hpp$")
    foreach (SOURCE ${RC_ARGS_SOURCES})
        string(REPLACE "${CMAKE_SOURCE_DIR}" "" SOURCE_REL "${SOURCE}")
        string(REGEX REPLACE "^[\/|\\]src[\/|\\]" "" SOURCE_REL "${SOURCE_REL}")

        cmake_path(GET SOURCE_REL STEM SOURCE_STEM)
        cmake_path(GET SOURCE_REL PARENT_PATH SOURCE_PARENT)

        set(SOURCE_CODEGEN_OUTPUT "${RC_ARGS_ROOT}/codegen/${SOURCE_PARENT}/${SOURCE_STEM}.hpp")
        message(STATUS "codegen: generating for ${SOURCE} to ${SOURCE_CODEGEN_OUTPUT}...")
        if (CLANG_FORMAT_FOUND)
            execute_process(COMMAND ${VENV_Python3_EXECUTABLE}
                    codegen.py ${SOURCE}
                    -c "codegen.cfg.xml"
                    -o "${SOURCE_CODEGEN_OUTPUT}"
                    --include "<${SOURCE_REL}>"
                    --format
                    --formatter "${CLANG_FORMAT_EXECUTABLE}"
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/codegen"
            )
        else ()
            execute_process(COMMAND ${VENV_Python3_EXECUTABLE}
                    codegen.py ${SOURCE}
                    -c "codegen.cfg.xml"
                    -o "${SOURCE_CODEGEN_OUTPUT}"
                    --include "<${SOURCE_REL}>"
                    --namespace "${SOURCE_STEM}"
                    --type-list "${SOURCE_STEM}"
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/codegen"
                    COMMAND_ERROR_IS_FATAL ANY
            )
        endif ()
    endforeach ()
endfunction()
