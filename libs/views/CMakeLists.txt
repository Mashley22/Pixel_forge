add_library(PixelForge_views OBJECT)

target_link_libraries(PixelForge_views
  PRIVATE
    PixelForge_core
)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_views
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

target_include_directories(PixelForge_views
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(utils)
add_subdirectory(ring_queue)

target_cxx_modules_public(PixelForge_views
  views.cppm
)
