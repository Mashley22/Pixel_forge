target_cxx_modules_public(PixelForge_core
  modules/exception.cppm
  modules/memzero.cppm
  modules/mem.cppm
  modules/align.cppm
  modules/linear_allocator.cppm
)

target_sources(PixelForge_core
  PRIVATE
    src/exception.cpp
    src/memzero.cpp
)
