cmake_minimum_required(VERSION 3.0)
#include_directories(googletest/googletest/include
#)
function(add_test)
  cmake_parse_arguments(ADDTEST "" "TARGET;SRC;LIB" "" ${ARGN})
  file(GLOB SOURCES ${ADDTEST_SRC})
  add_executable(${ADDTEST_TARGET} ${SOURCES})
  target_link_libraries(${ADDTEST_TARGET} ${ADDTEST_LIB} gtest)
  if (ADDTEST_LIB)
    add_dependencies(${ADDTEST_TARGET} ${ADDTEST_LIB})
  endif(ADDTEST_LIB)
endfunction(add_test)

add_test(TARGET "tags_tests" SRC "tags/*.cpp" LIB "tags")
add_test(TARGET "tags_cache_tests" SRC "tags_cache/*.cpp" LIB "tags")
add_test(TARGET "tags_name_path_line_tests" SRC "tags_name_path_line/*.cpp" LIB "tags")
add_test(TARGET "tag_view_tests" SRC "tag_view/*.cpp" LIB "tags")
add_test(TARGET "tags_repository_storage_tests" SRC "tags_repository_storage/*.cpp" LIB "tags")
add_test(TARGET "safe_call_tests" SRC "safe_call/*.cpp" LIB "facade")

#NOTE: this option is in fact disables overriding CMAKE_CXX_ flags inside googletest cmake scripts and allow to control runtime by global flags
option(gtest_force_shared_crt "Use shared (DLL) run-time lib even when Google Test is built as static lib." ON)
add_subdirectory(googletest)
add_subdirectory(facade)
