add_library(PixelForge_containers_lib)

target_link_libraries(PixelForge_containers_lib
  PUBLIC
    PixelForge_core
    PixelForge_containers
)

function(add_containers_perf_val VAL_NAME) 

  add_executable(${VAL_NAME})

  target_link_libraries(${VAL_NAME}
    PRIVATE
      PixelForge_containers_lib
  )

  add_perf_val(${VAL_NAME})

endfunction()

add_subdirectory(ring_queue)
