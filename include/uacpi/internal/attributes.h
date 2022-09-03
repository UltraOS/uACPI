#pragma once

#ifdef _MSC_VER
#define UACPI_ALWAYS_INLINE __forceinline

#define UACPI_PACKED(decl)  \
    __pragma(pack(push, 1)) \
    decl                    \
    __pragma(pack(pop))
#else
#define UACPI_ALWAYS_INLINE inline __attribute__((always_inline))
#define UACPI_PACKED(decl) decl __attribute__((packed))
#endif
