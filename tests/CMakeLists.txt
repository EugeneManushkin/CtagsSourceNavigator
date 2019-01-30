cmake_minimum_required(VERSION 3.0)
include_directories(googletest/googletest/include
                    ../src)
file(GLOB TAGS_SOURCES
    "tags/*.cpp"
    "../src/tags.cpp"
    "../src/tags.h"
    "../src/RegExp.cpp"
    "../src/RegExp.hpp"
)

add_executable(tags_tests ${TAGS_SOURCES})
target_link_libraries(tags_tests gtest)

file(GLOB TAGS_CACHE_SOURCES
    "tags_cache/*.cpp"
    "../src/tags_cache.cpp"
    "../src/tags_cache.h"
)

add_executable(tags_cache_tests ${TAGS_CACHE_SOURCES})
target_link_libraries(tags_cache_tests gtest)

#NOTE: this option is in fact disables overriding CMAKE_CXX_ flags inside googletest cmake scripts and allow to control runtime by global flags
option(gtest_force_shared_crt "Use shared (DLL) run-time lib even when Google Test is built as static lib." ON)
add_subdirectory(googletest)