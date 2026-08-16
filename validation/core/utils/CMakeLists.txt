add_executable(test_fmt
  test_fmt.cpp
)

add_executable(test_strcpy
  test_strcpy.cpp
)

add_core_unit_test(test_strcpy)
add_core_unit_test(test_fmt)
