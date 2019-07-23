file(GLOB SOURCES
    "src/*"
)

add_library(platform STATIC ${SOURCES})
target_include_directories(platform PUBLIC
    include
)
