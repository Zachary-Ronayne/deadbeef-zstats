cmake_minimum_required(VERSION 3.14)
project(zstat LANGUAGES C VERSION 1.0 DESCRIPTION "DeaDBeeF zstat Plugin")

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

# Add zstat plugin diles
add_library(zstat SHARED zstat.c zstat_db.c utils.c)
set_property(TARGET zstat PROPERTY C_STANDARD 99)

# Ensure glib is included, used for the timer to run
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
target_link_libraries(zstat PRIVATE PkgConfig::GLIB)

# Add sqlite
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
target_link_libraries(zstat PRIVATE PkgConfig::SQLITE3)

set(CMAKE_C_FLAGS_DEBUG "-g -Og -Wall -pedantic -DDEBUG")
target_compile_options(zstat PRIVATE
        -DDDB_API_LEVEL=10 -DDDB_WARN_DEPRECATED=1
        -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR})

# Uncomment for debug logs
# target_compile_definitions(zstat PRIVATE DEBUG)

# Name the library zstat
set_target_properties(zstat PROPERTIES PREFIX "")

# Install the library in the deadbeef lib folder
set(DEADBEEF_LIBDIR "~/.local/lib/deadbeef/")
message(STATUS "Install path: \"${DEADBEEF_LIBDIR}\"")
install(TARGETS zstat LIBRARY DESTINATION ${DEADBEEF_LIBDIR})
