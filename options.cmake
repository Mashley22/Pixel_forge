option(PIXELFORGE_TEST OFF "Test build")

option(PIXELFORGE_AGGRESSIVE_OPTIMISATIONS "Enable -O3 with LTO" OFF)

if(PIXELFORGE_TEST)
  add_compile_definitions(PIXELFORGE_TEST)
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
