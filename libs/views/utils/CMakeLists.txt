target_sources(PixelForge_views
  PUBLIC
    FILE_SET public_cxx_modules TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/..
    FILES traits.cppm
) # YAY CMAKE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
