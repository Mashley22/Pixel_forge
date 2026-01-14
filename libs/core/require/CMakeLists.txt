target_sources(PixelForge_core
  PUBLIC
    FILE_SET public_cxx_modules TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/..
    FILES require.cppm
) # YAY CMAKE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

target_sources(PixelForge_core
  PRIVATE
    require.cpp
)
  
if(PIXELFORGE_TEST)
  target_compile_definitions(PixelForge_core
    PUBLIC
      PIXELFORGE_REQUIRE_THROWS_ON_FAILURE
  )
endif()
