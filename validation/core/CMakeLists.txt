add_library(PixelForge_core_lib)

target_link_libraries(PixelForge_core_lib
  PUBLIC
    PixelForge_core
)

function(add_core_unit_test TEST_NAME)

  target_link_libraries(${TEST_NAME}
    PUBLIC
      PixelForge_core_lib
  )

  add_unit_test(${TEST_NAME})

endfunction()

add_subdirectory(require)
add_subdirectory(meta)
add_subdirectory(math)
