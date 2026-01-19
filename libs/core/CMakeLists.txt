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

add_subdirectory(require) # require must be first for the stupid cmake file set stuff
add_subdirectory(status)
add_subdirectory(exception)
add_subdirectory(utils)
add_subdirectory(meta)
add_subdirectory(mem)
