add_library(PixelForge_containers_lib)

target_link_libraries(PixelForge_containers_lib
  PUBLIC
    PixelForge_core
    PixelForge_containers
)

function(add_containers_unit_test TEST_NAME) 

  add_executable(${TEST_NAME})

  target_link_libraries(${TEST_NAME}
    PRIVATE
      PixelForge_containers_lib
  )

  add_unit_test(${TEST_NAME})

endfunction()

add_subdirectory(stack)
add_subdirectory(queue)
