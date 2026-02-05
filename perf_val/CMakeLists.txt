function(add_perf_val VAL_NAME)

  target_link_libraries(${VAL_NAME}
    PRIVATE
      Catch2::Catch2WithMain
      benchpp
  )

  catch_discover_tests(${VAL_NAME})

  target_compile_options(${VAL_NAME}
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )

endfunction()

add_subdirectory(adapters)
