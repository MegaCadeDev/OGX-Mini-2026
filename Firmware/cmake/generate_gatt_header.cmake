function(generate_gatt_header BTSTACK_ROOT GATT_INPUT_PATH GATT_HEADER_OUTPUT_PATH)
    set(SCRIPT_PATH ${BTSTACK_ROOT}/tool/compile_gatt.py)

    if(NOT EXISTS ${SCRIPT_PATH})
        message(FATAL_ERROR "GATT compilation script not found: ${SCRIPT_PATH}")
    endif()

    # Re-run CMake (and this check) whenever the .gatt file is edited.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GATT_INPUT_PATH})

    # Regenerate when the .gatt file changed, not only when the header
    # is missing (a stale header left new characteristics undeclared in existing build trees).
    if(EXISTS ${GATT_HEADER_OUTPUT_PATH} AND NOT ${GATT_INPUT_PATH} IS_NEWER_THAN ${GATT_HEADER_OUTPUT_PATH})
        message(STATUS "GATT header file up to date: ${GATT_HEADER_OUTPUT_PATH}")
        return()
    endif()

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    if(Python3_Interpreter_FOUND)
        message(STATUS "Python3 found: ${Python3_EXECUTABLE}")
        message(STATUS "Python3 version: ${Python3_VERSION}")
    else()
        message(FATAL_ERROR "Python3 not found! Python is required to integrate BTStack with ESP-IDF")
    endif()

    message(STATUS "Generating GATT header file: ${GATT_HEADER_OUTPUT_PATH}")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${SCRIPT_PATH} ${GATT_INPUT_PATH} ${GATT_HEADER_OUTPUT_PATH}
        WORKING_DIRECTORY ${BTSTACK_ROOT}
        RESULT_VARIABLE PYTHON_RESULT
        OUTPUT_VARIABLE PYTHON_OUTPUT
        ERROR_VARIABLE PYTHON_ERROR
    )

    if(PYTHON_RESULT EQUAL 0)
        message(STATUS "GATT header file generated successfully:\n${PYTHON_OUTPUT}")
    else()
        message(FATAL_ERROR "GATT header file generation failed:\n${PYTHON_ERROR}")
    endif()
endfunction()