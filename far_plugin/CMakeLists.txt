file(GLOB SOURCES
    "src/*"
)

add_library(far_plugin STATIC ${SOURCES})
add_dependencies(far_plugin platform)
target_include_directories(far_plugin PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    facade/include
    platform/include
)
target_compile_definitions(far_plugin
                           PUBLIC CTAGS_VERSION_MAJOR=${CTAGS_VERSION_MAJOR}
                           PUBLIC CTAGS_VERSION_MINOR=${CTAGS_VERSION_MINOR}
                           PUBLIC CTAGS_BUILD=${CTAGS_BUILD}
                           PUBLIC CTAGS_UTIL_DIR="${CTAGS_UTIL_DIR}"
                           PUBLIC CTAGS_GIT_COMMIT="${CTAGS_GIT_COMMIT}"
)

add_subdirectory(platform)
add_subdirectory(facade)
