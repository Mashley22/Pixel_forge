target_link_libraries(PixelForge_log_backends
  PRIVATE
    PixelForge_containers
    PixelForge_core
    PixelForge_mem
    PixelForge_logging
)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_log_backends
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

set(PixelForge_log_backends_sources
  log_backends.cppm
)

if(PIXELFORGE_ENABLE_FAST_LOG_BACKEND)
  list(INSERT PixelForge_log_backends_sources 0
    fast/fast.cppm
    fast/types.cppm
    fast/collector.cppm
  )
endif()


target_cxx_modules_public(PixelForge_log_backends
  ${PixelForge_log_backends_sources}
)
