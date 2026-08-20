cmake_minimum_required(VERSION 3.14)
project(zstat LANGUAGES C VERSION 2.0 DESCRIPTION "DeaDBeeF zstat Plugin")

unset(API CACHE)

# Place deadbeef install path here
set(${DEADBEEF_PATH} "/path/to/deadbeef")


find_file(API deadbeef.h PATHS DEADBEEF_PATH PATH_SUFFIXES deadbeef HINTS ~ ..)
if (NOT API)
    message(FATAL_ERROR "DeaDBeeF API: MISSING")
else()
    get_filename_component(API_DIR ${API} DIRECTORY)
    include_directories(${API_DIR})
    message(STATUS "DeaDBeeF API: present (${API})")
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
