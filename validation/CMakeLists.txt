function(add_unit_test TEST_NAME)
  target_link_libraries(${TEST_NAME}
    PRIVATE
      Catch2::Catch2WithMain
      PixelForge_validation_helpers
  )

  catch_discover_tests(${TEST_NAME})

  target_compile_options(${TEST_NAME}
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )

endfunction()

add_subdirectory(helpers)

add_subdirectory(core)
add_subdirectory(adapters)
add_subdirectory(mem)
