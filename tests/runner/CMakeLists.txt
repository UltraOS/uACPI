cmake_minimum_required(VERSION 3.16)

project(TestRunner C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../uacpi.cmake)

foreach(CONF_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONF_TYPE} CONF_TYPE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONF_TYPE} ${CMAKE_BINARY_DIR})
endforeach(CONF_TYPE ${CMAKE_CONFIGURATION_TYPES})

add_executable(
    test-runner
    test_runner.cpp
    helpers.cpp
    interface_impl.cpp
    ${UACPI_SOURCES}
)
target_include_directories(
    test-runner
    PRIVATE
    ${UACPI_INCLUDES}
)

# Address sanitizer on MSVC depends on a dynamic library that is not present in
# PATH by default. Lets just not enable it here.
if (NOT MSVC)
    target_compile_options(
        test-runner
        PRIVATE
        -fsanitize=address,undefined
    )
    target_link_options(
        test-runner
        PRIVATE
        -fsanitize=address,undefined
    )
endif ()
