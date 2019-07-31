if (WIN32)
    set(PLATFORM_DIR src/win)
else (WIN32)
    set(PLATFORM_DIR src/lx)
endif (WIN32)

file(GLOB SOURCES
    "src/*"
)

file(GLOB PLATFORM_SOURCES
    "${PLATFORM_DIR}/*"
)

add_library(platform STATIC ${SOURCES} ${PLATFORM_SOURCES})
target_include_directories(platform PRIVATE
    include
    ${PLATFORM_DIR}
)
