add_library(PixelForge_adapters_lib)

target_link_libraries(PixelForge_adapters_lib
  PUBLIC
    PixelForge_core
    PixelForge_adapters
)

function(add_adapters_perf_val VAL_NAME) 

  add_executable(${VAL_NAME})

  target_link_libraries(${VAL_NAME}
    PRIVATE
      PixelForge_adapters_lib
  )

  add_perf_val(${VAL_NAME})

endfunction()

add_subdirectory(ring_queue)
