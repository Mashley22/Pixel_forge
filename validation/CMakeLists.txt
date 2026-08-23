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
      -Wno-double-promotion # since some catch2 issues 
      -Wno-c2y-extensions
  )

endfunction()

add_subdirectory(helpers)

add_subdirectory(core)
add_subdirectory(adapters)
add_subdirectory(log)
add_subdirectory(mem)
