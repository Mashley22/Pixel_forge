function(add_unit_test TEST_NAME)
  target_link_libraries(${TEST_NAME}
    PRIVATE
      Catch2::Catch2WithMain
  )

  catch_discover_tests(${TEST_NAME})

  target_compile_options(${TEST_NAME}
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )

endfunction()

add_subdirectory(core)
