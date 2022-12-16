add_library(plugin INTERFACE)
target_include_directories(plugin INTERFACE include)

add_subdirectory(facade)
