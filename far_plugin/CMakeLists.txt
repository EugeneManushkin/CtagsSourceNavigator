file(GLOB SOURCES
    "src/*"
)

add_library(far_plugin STATIC ${SOURCES})
add_dependencies(far_plugin platform)
target_include_directories(far_plugin PUBLIC
    facade/include
)

add_subdirectory(platform)
add_subdirectory(facade)
