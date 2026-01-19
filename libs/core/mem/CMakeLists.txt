target_cxx_modules_public(PixelForge_core
  modules/memzero.cppm
  modules/mem.cppm
)

target_sources(PixelForge_core
  PRIVATE
    src/memzero.cpp
)
