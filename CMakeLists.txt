# since the c++ modules stuff
cmake_minimum_required(VERSION 3.28)

project(PixelForge VERSION 0.0.1 LANGUAGES CXX)

set(PIXELFORGE_MINIMUM_CXX_STANDARD 20)

if(NOT DEFINED CMAKE_CXX_STANDARD)
  message(STATUS "No c++ standard found, defaulting to ${PIXELFORGE_MINIMUM_CXX_STANDARD}")
  set(CMAKE_CXX_STANDARD ${PIXELFORGE_MINIMUM_CXX_STANDARD})
elseif(CMAKE_CXX_STANDARD LESS ${PIXELFORGE_MINIMUM_CXX_STANDARD})
  message(FATAL_ERROR "c++ standard set too low: ${CMAKE_CXX_STANDARD}. Need ${PIXELFORGE_MINIMUM_CXX_STANDARD} atleast!")
endif()

include(options.cmake)
include(cmake/utils.cmake)

add_subdirectory(libs)

if(PIXELFORGE_TEST)
  add_compile_definitions(PIXELFORGE_TEST)
  enable_testing()
  include(CTest)

  add_subdirectory(extern/catch2)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/extern/catch2/extras")
  include(Catch)

  add_subdirectory(validation)
endif()
