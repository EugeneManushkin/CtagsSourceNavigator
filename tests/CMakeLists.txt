cmake_minimum_required(VERSION 3.0)
include_directories(googletest/googletest/include
                    ../src)
file(GLOB TAGS_SOURCES
    "tags/*.cpp"
    "../src/tags.cpp"
    "../src/tags.h"
    "../src/tags_cache.cpp"
    "../src/tags_cache.h"
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


file(GLOB TAGS_CACHE_SOURCES
    "tag_view/*.cpp"
    "../src/tag_view.cpp"
)

add_executable(tag_view_tests ${TAGS_CACHE_SOURCES})
target_link_libraries(tag_view_tests gtest)

file(GLOB COMPOSITE_TASK_SOURCES
    "composite_task/*.cpp"
    "../far_plugin/src/composite_task.cpp"
)

add_executable(composite_task_tests ${COMPOSITE_TASK_SOURCES})
target_link_libraries(composite_task_tests gtest)
target_include_directories(composite_task_tests PUBLIC ../far_plugin/src)

add_executable(mock_process process/mock_process/main.cpp)
file(GLOB PROCESS_SOURCES
    "process/*.cpp"
)
add_executable(process_tests ${PROCESS_SOURCES})
target_link_libraries(process_tests platform gtest)
target_include_directories(process_tests PUBLIC ../far_plugin/platform/include)

#NOTE: this option is in fact disables overriding CMAKE_CXX_ flags inside googletest cmake scripts and allow to control runtime by global flags
option(gtest_force_shared_crt "Use shared (DLL) run-time lib even when Google Test is built as static lib." ON)
add_subdirectory(googletest)