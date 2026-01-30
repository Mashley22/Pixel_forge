add_library(PixelForge_views_lib)

target_link_libraries(PixelForge_views_lib
  PUBLIC
    PixelForge_core
    PixelForge_views
)

function(add_views_unit_test TEST_NAME) 

  add_executable(${TEST_NAME})

  target_link_libraries(${TEST_NAME}
    PRIVATE
      PixelForge_views_lib
  )

  add_unit_test(${TEST_NAME})

endfunction()

add_subdirectory(ring_queue)
