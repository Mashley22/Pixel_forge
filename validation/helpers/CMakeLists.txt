add_library(PixelForge_validation_helpers)

target_sources(PixelForge_validation_helpers
  PUBLIC
    FILE_SET public_cxx_modules TYPE CXX_MODULES
    FILES helpers.cppm
)
