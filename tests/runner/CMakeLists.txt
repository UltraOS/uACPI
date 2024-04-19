cmake_minimum_required(VERSION 3.16)

project(TestRunner C CXX)

set(CMAKE_CXX_STANDARD 17)
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
    resource_tests.cpp
    ${UACPI_SOURCES}
)
target_include_directories(
    test-runner
    PRIVATE
    ${UACPI_INCLUDES}
)

if (NOT REDUCED_HARDWARE_BUILD)
    set(REDUCED_HARDWARE_BUILD 0)
endif()

if (NOT SIZED_FREES_BUILD)
    set(SIZED_FREES_BUILD 1)
endif()

target_compile_definitions(
    test-runner
    PRIVATE
    -DUACPI_REDUCED_HARDWARE=${REDUCED_HARDWARE_BUILD}
    -DUACPI_SIZED_FREES=${SIZED_FREES_BUILD}
)

if (MSVC)
    # Address sanitizer on MSVC depends on a dynamic library that is not present in
    # PATH by default. Lets just not enable it here.
    target_compile_options(
        test-runner
        PRIVATE
        /W3 /WX
        /wd4200 /wd4267 /wd4244
    )
else ()
    target_compile_options(
        test-runner
        PRIVATE
        -fsanitize=address,undefined -g3 -Wall -Wextra -Werror
    )
    target_link_options(
        test-runner
        PRIVATE
        -fsanitize=address,undefined -g3
    )
endif ()
