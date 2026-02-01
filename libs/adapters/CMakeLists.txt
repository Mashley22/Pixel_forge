add_library(PixelForge_adapters OBJECT)

target_link_libraries(PixelForge_adapters
  PRIVATE
    PixelForge_core
)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_adapters
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

target_include_directories(PixelForge_adapters
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(utils)
add_subdirectory(ring_queue)

target_cxx_modules_public(PixelForge_adapters
  adapters.cppm
)
