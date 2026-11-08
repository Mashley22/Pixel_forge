add_library(PixelForge_log_backends_lib)

target_link_libraries(PixelForge_log_backends_lib
  PUBLIC
    PixelForge_log_backends
    PixelForge_logging
    PixelForge_containers
    PixelForge_mem
    PixelForge_core
)

function(add_log_backend_unit_test TEST_NAME)

  add_executable(${TEST_NAME})

  target_link_libraries(${TEST_NAME}
    PUBLIC
      PixelForge_log_backends_lib
  )

  add_unit_test(${TEST_NAME})

endfunction()

if(PIXELFORGE_ENABLE_FAST_LOG_BACKEND)
  add_subdirectory(fast)
endif()
