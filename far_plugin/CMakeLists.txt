aux_source_directory(src SOURCES)
add_library(plugin ${SOURCES})
target_include_directories(plugin PUBLIC include)

add_subdirectory(facade)
