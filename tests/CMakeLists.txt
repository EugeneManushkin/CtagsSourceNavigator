cmake_minimum_required(VERSION 3.0)

add_executable(tag_view_tests tag_view/main.cpp)
target_link_libraries(tag_view_tests tags gtest)

add_executable(tags_tests tags/main.cpp)
target_link_libraries(tags_tests tags gtest)

add_executable(tags_cache_tests tags_cache/main.cpp)
target_link_libraries(tags_cache_tests tags gtest)

add_executable(tags_name_path_line_tests tags_name_path_line/main.cpp)
target_link_libraries(tags_name_path_line_tests tags gtest)

add_executable(tags_repository_storage_tests tags_repository_storage/main.cpp)
target_link_libraries(tags_repository_storage_tests tags gtest)

#NOTE: this option is in fact disables overriding CMAKE_CXX_ flags inside googletest cmake scripts and allow to control runtime by global flags
option(gtest_force_shared_crt "Use shared (DLL) run-time lib even when Google Test is built as static lib." ON)
add_subdirectory(googletest)
add_subdirectory(plugin)
