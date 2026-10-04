export module PixelForge.logging;

export import :record;
export import :backend.console;

#ifdef PIXELFORGE_ENABLE_FAST_LOG_BACKEND
export import :backend.fast;
#endif
