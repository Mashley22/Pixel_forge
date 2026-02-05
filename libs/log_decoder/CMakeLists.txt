add_library(PixelForge_log_decoder OBJECT)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_core
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

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
