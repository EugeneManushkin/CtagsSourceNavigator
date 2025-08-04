add_executable(break_keys break_keys/main.cpp)
target_link_libraries(break_keys ctags_static gtest)

add_executable(guid guid/main.cpp)
target_link_libraries(guid ctags_static gtest)
