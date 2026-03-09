add_library(PixelForge_adapters_lib)

target_link_libraries(PixelForge_adapters_lib
  PUBLIC
    PixelForge_core
    PixelForge_adapters
)

function(add_adapters_unit_test TEST_NAME) 

  add_executable(${TEST_NAME})

  target_link_libraries(${TEST_NAME}
    PRIVATE
      PixelForge_adapters_lib
  )

  add_unit_test(${TEST_NAME})

endfunction()

add_subdirectory(ring_queue)
add_subdirectory(stack)
