add_library(PixelForge_core)

target_include_directories(PixelForge_core
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(assert)
add_subdirectory(status)
add_subdirectory(exception)
