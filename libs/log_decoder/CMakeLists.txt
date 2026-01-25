add_library(PixelForge_log_decoder OBJECT)

target_link_libraries(PixelForge_log_decoder
  PRIVATE
    PixelForge_logging
    PixelForge_core
)

target_cxx_modules_public(PixelForge_log_decoder
  log_decoder.cppm
)

target_sources(PixelForge_log_decoder
  PRIVATE
    log_decoder.cpp
)
