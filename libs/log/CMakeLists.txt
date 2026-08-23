add_library(PixelForge_logging OBJECT)

target_link_libraries(PixelForge_logging
  PRIVATE
    PixelForge_core
)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_logging
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )

  # -Wstrict-overflow fires inside libstdc++'s chrono formatter
  # (__formatter_chrono / bits/chrono.h) when GCC instantiates the
  # timestamp formatting in the console backend. The diagnostic lands in
  # system-header code (or at end-of-TU under -O0), so it cannot be scoped
  # with pragmas from module purview; disable it just for this file.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(modules/backends/console.cppm PROPERTIES
      COMPILE_OPTIONS "-Wno-strict-overflow"
    )
  endif()
endif()

target_cxx_modules_public(PixelForge_logging
  modules/record.cppm
  modules/backends/console.cppm
  logging.cppm
)
