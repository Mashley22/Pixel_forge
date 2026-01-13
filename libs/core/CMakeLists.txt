add_library(PixelForge_core)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_core
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

target_include_directories(PixelForge_core
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(require)
add_subdirectory(status)
add_subdirectory(exception)
