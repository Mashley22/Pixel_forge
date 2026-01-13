add_executable(test_assert
  test_assert.cpp
)

target_link_libraries(test_assert
  PRIVATE
  PixelForge_core
)

add_unit_test(test_assert)
