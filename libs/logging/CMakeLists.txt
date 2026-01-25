add_library(PixelForge_logging OBJECT)

target_link_libraries(PixelForge_logging
  PRIVATE
    PixelForge_core
)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_logging
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

target_cxx_modules_public(PixelForge_logging
  modules/log_codes.cppm
)


