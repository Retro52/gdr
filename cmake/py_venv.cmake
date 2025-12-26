function(create_venv)
    cmake_parse_arguments(CV_ARGS "" "PATH" "DEPENDENCIES" "${ARGN}")
    if (WIN32)
        set(VENV_Python3_EXECUTABLE "${CV_ARGS_PATH}/Scripts/python.exe")
    else ()
        set(VENV_Python3_EXECUTABLE "${CV_ARGS_PATH}/Scripts/python")
    endif ()

    if (EXISTS ${VENV_Python3_EXECUTABLE})
        message(STATUS
                "create_venv: venv seems to already exist at ${VENV_Python3_EXECUTABLE} (to force update remove the directory)")
        set(VENV_Python3_EXECUTABLE "${VENV_Python3_EXECUTABLE}" PARENT_SCOPE)
        return()
    endif ()

    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    set(SYSTEM_Python3_EXECUTABLE "${Python3_EXECUTABLE}")
    message(STATUS "create_venv: found system python at ${SYSTEM_Python3_EXECUTABLE}")

    message(STATUS "create_venv: running -m venv command...")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -m venv "${CV_ARGS_PATH}" COMMAND_ERROR_IS_FATAL ANY)

    if (NOT EXISTS "${VENV_Python3_EXECUTABLE}")
        message(FATAL_ERROR "Failed to create venv: ${VENV_Python3_EXECUTABLE} does not exist")
    else ()
        message(STATUS "create_venv: venv successfully created at ${CV_ARGS_PATH}")
    endif ()

    message(STATUS "create_venv: installing dependencies...")
    foreach (DEPENDENCY ${CV_ARGS_DEPENDENCIES})
        message(STATUS "create_venv: installing ${DEPENDENCY}...")
        execute_process(COMMAND "${VENV_Python3_EXECUTABLE}" -m pip install "${DEPENDENCY}"
                OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
    endforeach ()

    set(VENV_Python3_EXECUTABLE "${VENV_Python3_EXECUTABLE}" PARENT_SCOPE)
    message(STATUS "create_venv: dependencies ${CV_ARGS_DEPENDENCIES} installed.")
    message(STATUS "create_venv: venv creation done!")
endfunction()
