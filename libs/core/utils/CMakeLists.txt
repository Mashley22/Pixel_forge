target_cxx_modules_public(PixelForge_core
  modules/char_encoding.cppm
  modules/utils.cppm
)

target_sources(PixelForge_core
  PRIVATE
    src/char_encoding.cpp
)
