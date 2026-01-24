add_subdirectory(memzero)

add_executable(test_align
  test_align.cpp
)

add_core_unit_test(test_align)
