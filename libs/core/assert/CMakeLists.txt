target_sources(PixelForge_core
  PUBLIC
    FILE_SET public_cxx_modules TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/..
    FILES assert.cppm
) # YAY CMAKE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  
target_sources(PixelForge_core
  PRIVATE
    assert.cpp
)
