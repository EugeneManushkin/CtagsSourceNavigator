file(GLOB SOURCES
    "src/*.h"
    "src/*.cpp"
    "src/exported_calls.def"
)

if (MSVC)
    set(SOURCES ${SOURCES} "src/resource.rc")
endif (MSVC)

add_library(ctags SHARED ${SOURCES})
target_link_libraries(ctags tags far_plugin)
target_include_directories(ctags PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/far_plugin/facade/src
)

target_compile_definitions(ctags PUBLIC CTAGS_VERSION_MAJOR=${CTAGS_VERSION_MAJOR}
                                 PUBLIC CTAGS_VERSION_MINOR=${CTAGS_VERSION_MINOR}
                                 PUBLIC CTAGS_BUILD=${CTAGS_BUILD}
                                 PUBLIC CTAGS_UTIL_DIR="${CTAGS_UTIL_DIR}"
                                 PUBLIC CTAGS_GIT_COMMIT="${CTAGS_GIT_COMMIT}"
)

set(PLUGIN_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/ctags)
file(GLOB CONFIG_FILES "${CMAKE_SOURCE_DIR}/far_plugin/config/*")
add_custom_command(TARGET ctags POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CONFIG_FILES} ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ctags> ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/COPYING ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/README.md ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/README-rus.md ${PLUGIN_DIR}
)
