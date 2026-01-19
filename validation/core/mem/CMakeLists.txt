add_executable(test_memzero
  test_memzero.cpp
)

add_core_unit_test(test_memzero)

function(add_memzero_explicit_test TEST_NAME)

add_executable(${TEST_NAME}
  test_memzero_explicit.cpp
  stupid_memzero_explicit_dead_var.cpp
)

add_core_unit_test(${TEST_NAME})

target_compile_options(${TEST_NAME}
  PRIVATE
    ${ARGN}
)

string(JOIN " " ARGN_STRING ${ARGN})

target_compile_definitions(${TEST_NAME}
  PRIVATE
    BUILD_OPTS="${ARGN_STRING}"
)

endfunction()

if(MSVC)
  add_memzero_explicit_test(test_Od "/Od")
  add_memzero_explicit_test(test_O1_msvc "/O1") 
  add_memzero_explicit_test(test_O2_msvc "/O2") 
  add_memzero_explicit_test(test_Ox_msvc "/Ox")  
  add_memzero_explicit_test(test_O2_GL "/O2 /GL") 
  add_memzero_explicit_test(test_0x_GL "/0x /GL")
else()
  add_memzero_explicit_test(test_O0 "-O0")
  add_memzero_explicit_test(test_O1 "-O1")
  add_memzero_explicit_test(test_O2 "-O2")
  add_memzero_explicit_test(test_O3 "-O3")
  add_memzero_explicit_test(test_Os "-Os")
  add_memzero_explicit_test(test_Og "-Og" "-g")
endif()
