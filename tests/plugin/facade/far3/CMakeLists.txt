add_executable(break_keys_tests break_keys/main.cpp)
target_link_libraries(break_keys_tests ctags_static gtest)

add_executable(guid_tests guid/main.cpp)
target_link_libraries(guid_tests ctags_static gtest)

add_executable(config_stream_reader_tests config_stream_reader/main.cpp)
target_link_libraries(config_stream_reader_tests ctags_static gtest)
