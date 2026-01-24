add_library(PixelForge_mem)

target_link_libraries(PixelForge_mem
  PRIVATE
    PixelForge_core
)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_mem
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

target_cxx_modules_public(PixelForge_mem
  modules/exception.cppm
  modules/memzero.cppm
  modules/mem.cppm
  modules/align.cppm
  modules/linear_allocator.cppm
)

target_sources(PixelForge_mem
  PRIVATE
    src/exception.cpp
    src/memzero.cpp
)
