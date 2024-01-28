#pragma once

/*
 * Compiler-specific attributes/macros go here. This is the default placeholder
 * that should work for MSVC/GCC/clang.
 */

#define UACPI_ALIGN(x) __declspec(align(x))

#ifdef _MSC_VER
    #include <intrin.h>

    #define UACPI_ALWAYS_INLINE __forceinline

    #define UACPI_PACKED(decl)  \
        __pragma(pack(push, 1)) \
        decl;                   \
        __pragma(pack(pop))
#else
    #define UACPI_ALWAYS_INLINE inline __attribute__((always_inline))
    #define UACPI_PACKED(decl) decl __attribute__((packed));
#endif

#ifdef __GNUC__
    #define uacpi_unlikely(expr) __builtin_expect(!!(expr), 0)
    #define uacpi_likely(expr)   __builtin_expect(!!(expr), 1)

    #if __has_attribute(__fallthrough__)
        #define UACPI_FALLTHROUGH __attribute__((__fallthrough__))
    #endif
#else
    #define uacpi_unlikely(expr) expr
    #define uacpi_likely(expr)   expr
#endif

#ifndef UACPI_FALLTHROUGH
    #define UACPI_FALLTHROUGH do {} while (0)
#endif

#ifndef UACPI_POINTER_SIZE
    #ifdef _WIN32
        #ifdef _WIN64
            #define UACPI_POINTER_SIZE 8
        #else
            #define UACPI_POINTER_SIZE 4
        #endif
    #endif

    #ifdef __GNUC__
        #if __x86_64__ || defined(__aarch64__)
            #define UACPI_POINTER_SIZE 8
        #elif defined(__i386__) || defined(__arm__)
            #define UACPI_POINTER_SIZE 4
        #endif
    #endif
#endif

#ifndef UACPI_POINTER_SIZE
#error Failed to detect pointer size
#endif
