add_library(PixelForge_validation_helpers)

target_include_directories(PixelForge_validation_helpers
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(PixelForge_validation_helpers
  PUBLIC
    FILE_SET public_cxx_modules TYPE CXX_MODULES
    FILES helpers.cppm
)
