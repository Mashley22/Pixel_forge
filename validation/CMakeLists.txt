function(add_unit_test TEST_NAME)
  target_link_libraries(${TEST_NAME}
    PRIVATE
      Catch2::Catch2WithMain
  )

  catch_discover_tests(${TEST_NAME})

endfunction()

add_subdirectory(core)
