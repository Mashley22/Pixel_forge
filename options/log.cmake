option(PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE "Define the size of the logging buffer used when requireFail policy is set to log and continue" 64)

option(PIXELFORGE_ENABLE_FAST_LOG_BACKEND "Enable the fast logger backend" ON)

target_compile_definitions(PixelForge_core PRIVATE
  PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE ${PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE}
)

if(PIXELFORGE_ENABLE_FAST_LOG_BACKEND) 
  target_compile_definitions(PixelForge_log PUBLIC 
    PIXELFORGE_ENABLE_FAST_LOG_BACKEND
  )
endif()

target_compile_definitions(PixelForge_core PUBLIC 
  PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE ${PIXELFORGE_REQUIRE_FAIL_LOG_BUF_SIZE}
)
