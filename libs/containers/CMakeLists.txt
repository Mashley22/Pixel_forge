target_link_libraries(PixelForge_containers
  PRIVATE
    PixelForge_core
)

if(PIXELFORGE_TEST)
  target_compile_options(PixelForge_containers
    PRIVATE
      ${PEDANTIC_COMPILE_OPTIONS}
  )
endif()

target_include_directories(PixelForge_containers
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(utils)
add_subdirectory(stack)
add_subdirectory(queue)

target_cxx_modules_public(PixelForge_containers
  containers.cppm
)
