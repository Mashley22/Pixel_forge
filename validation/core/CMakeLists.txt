function(add_core_unit_test TEST_NAME)

  target_link_libraries(${TEST_NAME}
    PUBLIC
      PixelForge_core
  )

  add_unit_test(${TEST_NAME})

endfunction()

add_subdirectory(require)
add_subdirectory(meta)
add_subdirectory(mem)
