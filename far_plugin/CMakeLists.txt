file(GLOB SOURCES
    "src/*"
)

add_library(far_plugin STATIC ${SOURCES})
target_include_directories(far_plugin PUBLIC
    facade/include
)

add_subdirectory(facade)
