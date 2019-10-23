cmake_minimum_required(VERSION 3.0)
include_directories(googletest/googletest/include
                    ../src)
function(add_test)
  cmake_parse_arguments(ADDTEST "" "TARGET;SRC;LIB" "INCLUDES" ${ARGN})
  file(GLOB SOURCES ${ADDTEST_SRC})
  add_executable(${ADDTEST_TARGET} ${SOURCES})
  target_link_libraries(${ADDTEST_TARGET} ${ADDTEST_LIB} gtest)
  add_dependencies(${ADDTEST_TARGET} ${ADDTEST_LIB})
  foreach(INC ${ADDTEST_INCLUDES})
    target_include_directories(${ADDTEST_TARGET} PUBLIC ${INC})
  endforeach()
endfunction(add_test)

add_test(TARGET "tags_tests" SRC "tags/*.cpp" LIB "tags" INCLUDES "")
add_test(TARGET "tags_cache_tests" SRC "tags_cache/*.cpp" LIB "tags" INCLUDES "")
add_test(TARGET "tag_view_tests" SRC "tag_view/*.cpp" LIB "tags" INCLUDES "")
add_test(TARGET "tags_repository_storage_tests" SRC "tags_repository_storage/*.cpp" LIB "tags" INCLUDES "")
add_test(TARGET "composite_task_tests" SRC "composite_task/*.cpp" LIB "far_plugin" INCLUDES "${CMAKE_SOURCE_DIR}/far_plugin/src")
add_test(TARGET "process_tests" SRC "process/*.cpp" LIB "platform" INCLUDES "${CMAKE_SOURCE_DIR}/far_plugin/platform/include")
add_test(TARGET "execute_process_task" SRC "execute_process_task/*.cpp" LIB "far_plugin" INCLUDES
         "${CMAKE_SOURCE_DIR}/far_plugin/platform/include"
         "${CMAKE_SOURCE_DIR}/far_plugin/src")

add_executable(mock_process process/mock_process/main.cpp)

#NOTE: this option is in fact disables overriding CMAKE_CXX_ flags inside googletest cmake scripts and allow to control runtime by global flags
option(gtest_force_shared_crt "Use shared (DLL) run-time lib even when Google Test is built as static lib." ON)
add_subdirectory(googletest)