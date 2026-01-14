add_executable(test_require
  test_require.cpp
)

target_link_libraries(test_require
  PRIVATE
    PixelForge_core
)

add_unit_test(test_require)
