cmake_minimum_required(VERSION 3.14)
project(zstat LANGUAGES C VERSION 2.0 DESCRIPTION "DeaDBeeF zstat Plugin")

unset(DEADBEEF_H CACHE)

find_file(DEADBEEF_H 
    NAMES deadbeef.h
    PATHS "${CMAKE_CURRENT_LIST_DIR}"
    NO_DEFAULT_PATH
)
if (NOT DEADBEEF_H)
    message(FATAL_ERROR "DeaDBeeF deadbeef.h missing, get it from the repo and add it to the root directory, repo: https://github.com/DeaDBeeF-Player/deadbeef ")
else()
    get_filename_component(DEADBEEF_DIR ${DEADBEEF_H} DIRECTORY)
    include_directories(${DEADBEEF_DIR})
    message(STATUS "deadbeef.h: present (${DEADBEEF_H})")
endif()

# Add zstat
add_library(zstat SHARED zstat.c)
set_property(TARGET zstat PROPERTY C_STANDARD 99)

set(CMAKE_C_FLAGS_DEBUG "-g -Og -Wall -pedantic -DDEBUG")
target_compile_options(zstat PRIVATE
        -DDDB_API_LEVEL=10 -DDDB_WARN_DEPRECATED=1
        -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR})

# Name the library zstat
set_target_properties(zstat PROPERTIES PREFIX "")

# Install the library in the deadbeef lib folder
set(DEADBEEF_LIBDIR "~/.local/lib/deadbeef/")
message(STATUS "Install path: \"${DEADBEEF_LIBDIR}\"")
install(TARGETS zstat LIBRARY DESTINATION ${DEADBEEF_LIBDIR})
