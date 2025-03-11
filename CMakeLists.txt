cmake_minimum_required(VERSION 3.0)

set (CTAGS_VERSION_MAJOR 0 CACHE STRING "Plugin major version")
set (CTAGS_VERSION_MINOR 0 CACHE STRING "Plugin minor version")
set (CTAGS_VERSION_REVISION 0 CACHE STRING "Plugin revision")
set (CTAGS_VERSION_BUILD 0 CACHE STRING "Plugin build number")
set (CTAGS_UTIL_DIR util CACHE STRING "Directory with downloaded ctags utility")

file(GLOB SOURCES
    "src/*"
)

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

add_library(tags STATIC ${SOURCES})

execute_process(COMMAND git rev-parse HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE CTAGS_GIT_COMMIT
                OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_subdirectory(far_plugin)
add_subdirectory(tests)
