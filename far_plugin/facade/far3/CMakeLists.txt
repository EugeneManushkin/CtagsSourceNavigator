file(GLOB SOURCES_STATIC
    "src/*.h"
    "src/*.cpp"
)

add_library(ctags_static ${SOURCES_STATIC})
target_link_libraries(ctags_static tags plugin facade)
target_include_directories(ctags_static PUBLIC include)
target_compile_definitions(ctags_static
                                 PUBLIC CTAGS_VERSION_MAJOR=${CTAGS_VERSION_MAJOR}
                                 PUBLIC CTAGS_VERSION_MINOR=${CTAGS_VERSION_MINOR}
                                 PUBLIC CTAGS_VERSION_REVISION=${CTAGS_VERSION_REVISION}
                                 PUBLIC CTAGS_VERSION_BUILD=${CTAGS_VERSION_BUILD}
                                 PUBLIC CTAGS_UTIL_DIR="${CTAGS_UTIL_DIR}"
                                 PUBLIC CTAGS_GIT_COMMIT="${CTAGS_GIT_COMMIT}"
)

set(SOURCES
    "src/exported_calls.def"
)

if (MSVC)
    set(SOURCES ${SOURCES} "src/resource.rc")
endif (MSVC)

add_library(ctags SHARED ${SOURCES})
target_link_libraries(ctags ctags_static)

set(PLUGIN_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/ctags CACHE STRING "Plugin release folder")
configure_file(src/tagseng.hlf.in tagseng.hlf)
file(GLOB README_FILES "${CMAKE_SOURCE_DIR}/README*.md")
add_custom_command(TARGET ctags POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy tagseng.hlf ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/src/tagseng.lng ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ctags> ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/COPYING ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${README_FILES} ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/far_plugin/hotkeys/ctags_hotkeys.lua ${PLUGIN_DIR}
)
