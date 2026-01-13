function(m_target_cxx_modules
  TARGET_NAME 
  SCOPE 
  MODULE_FILE_SET_NAME
  MODULE_FILES
)

  target_sources(${TARGET_NAME}
    ${SCOPE}
      FILE_SET ${MODULE_FILE_SET_NAME} TYPE CXX_MODULES 
      FILES ${MODULE_FILES}
  )
endfunction()

function(target_cxx_modules_public TARGET_NAME)

  set(MODULE_FILES ${ARGN})
    
  m_target_cxx_modules(${TARGET_NAME} "PUBLIC" "public_cxx_modules" ${MODULE_FILES})

endfunction()

function(target_cxx_modules_private TARGET_NAME)

  set(MODULE_FILES ${ARG_UNPARSED_ARGUMENTS})

  m_target_cxx_modules(${TARGET_NAME} PRIVATE private_cxx_modules ${MODULE_FILES})

endfunction()

set(PEDANTIC_COMPILE_OPTIONS
  -Wall 
  -Wextra 
  -Wpedantic 
  -Werror 
  -Wshadow 
  -Wconversion 
  -Wsign-conversion 
  -Wcast-align 
  -Wcast-qual 
  -Wformat=2 
  -Wundef 
  -Wnull-dereference 
  -Wdouble-promotion 
  -Wimplicit-fallthrough 
  -Wmissing-declarations 
  -Wredundant-decls 
  -Wstrict-overflow=5 
  -Wswitch-enum 
  -Wunreachable-code
)
