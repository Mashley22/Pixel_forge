add_library(PixelForge_log_lib)

target_link_libraries(PixelForge_log_lib
  PUBLIC
    PixelForge_logging
    PixelForge_core
)

function(add_log_unit_test TEST_NAME)

  add_executable(${TEST_NAME})

  target_link_libraries(${TEST_NAME}
    PUBLIC
      PixelForge_log_lib
  )

  add_unit_test(${TEST_NAME})

endfunction()

add_subdirectory(backends)
