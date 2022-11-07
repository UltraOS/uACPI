# This cmake file is not meant to be used to actually build the project.
# Instead, it is supposed to be included via 'include()' by the actual
# CMakeLists.txt file, which intends to use uACPI. This file takes care of
# accumulating all sources & includes into well-defined exported variables
# that the parent cmake file is supposed to make use of.

# ======== Exported public variables =========

# A list that contains all of the compilable files that must be compiled along
# with all other source files (added via target_sources etc).
set(UACPI_SOURCES "")

# A list that contains all of the include paths that must be appended to the
# parent include directories (via target_include_directories etc).
set(UACPI_INCLUDES "")

# absolute path to the uACPI root directory
set(UACPI_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")

# ============================================

list(APPEND UACPI_INCLUDES "${UACPI_ROOT_DIR}/include")

macro(uacpi_add_sources)
    foreach (SOURCE_FILE ${ARGV})
        list(APPEND UACPI_SOURCES "${CMAKE_CURRENT_LIST_DIR}/${SOURCE_FILE}")
    endforeach()
endmacro()

macro(uacpi_subdir SUBDIR)
    include("${UACPI_ROOT_DIR}/${SUBDIR}/files.cmake")
endmacro()

uacpi_subdir(source)
