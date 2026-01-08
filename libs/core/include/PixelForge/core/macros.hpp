#ifndef PIXELFORGE_CORE_MACROS_HPP
#define PIXELFORGE_CORE_MACROS_HPP

#include <new>

#ifdef PIXELFORGE_ASSERT_THROW
#define PF_NOEXCEPT 
#else
#define PF_NOEXCEPT noexcept
#endif

#define PF_CACHE_LINE_ALIGN alignas(std::hardware_destructive_interference_size)

#endif /* PIXELFORGE_CORE_MACROS_HPP */
