add_library(PixelForge_mem OBJECT)

target_link_libraries(PixelForge_mem
  PRIVATE
    PixelForge_adapters
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
  modules/align.cppm
  modules/linear_arena.cppm
  mem.cppm
)
