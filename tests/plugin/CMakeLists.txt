add_executable(config_data_mapper_tests config_data_mapper/main.cpp)
target_link_libraries(config_data_mapper_tests plugin gtest)

add_subdirectory(facade)
