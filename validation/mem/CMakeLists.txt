add_library(PixelForge_mem_lib)

target_link_libraries(PixelForge_mem_lib
  PUBLIC
    PixelForge_mem
    PixelForge_core
)

function(add_mem_unit_test TEST_NAME)

  target_link_libraries(${TEST_NAME}
    PUBLIC
      PixelForge_mem_lib
  )

  add_unit_test(${TEST_NAME})

endfunction()

add_subdirectory(memzero)

add_executable(test_align
  test_align.cpp
)

add_mem_unit_test(test_align)
