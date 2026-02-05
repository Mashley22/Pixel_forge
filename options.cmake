option(PIXELFORGE_TEST OFF "Test build")

option(PIXELFORGE_AGGRESSIVE_OPTIMISATIONS "Enable -O3 with LTO" OFF)

option(PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE "Define the size of the logging buffer used when requireFail policy is set to log and continue" 64)

option(PIXELFORGE_PERF_VAL "Performance validation" OFF)

add_compile_definitions(PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE ${PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE})

if(PIXELFORGE_TEST)
  add_compile_definitions(PIXELFORGE_TEST)
endif()

if(PIXELFORGE_PERF_VAL)
  add_compile_definitions(PIXELFORGE_TEST)
  if (NOT PIXELFORGE_AGGRESSIVE_OPTIMISATIONS)
    message(FATAL_ERROR "Please enable aggressive optimisations when building performance validations")
  endif()
endif()

if(PIXELFORGE_AGGRESSIVE_OPTIMISATIONS)
  add_compile_definitions(PIXELFORGE_AGGRESSIVE_OPTIMISATIONS)

  include(CheckIPOSupported)
  check_ipo_supported(RESULT supported OUTPUT error)

  if(supported)
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

  else()
    message(STATUS "IPO / LTO not supported: <${error}>")
  endif()

  if(MSVC)
    add_compile_options("/Ox")
  else()
    add_compile_options("-O3")
  endif()
endif()
