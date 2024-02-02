#pragma once

/*
 * Platform-specific types go here. This is the default placeholder using
 * types from the standard headers.
 */

#ifdef UACPI_OVERRIDE_TYPES
#include "uacpi_types.h"
#else

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef uint8_t uacpi_u8;
typedef uint16_t uacpi_u16;
typedef uint32_t uacpi_u32;
typedef uint64_t uacpi_u64;

typedef int8_t uacpi_i8;
typedef int16_t uacpi_i16;
typedef int32_t uacpi_i32;
typedef int64_t uacpi_i64;

#define UACPI_TRUE true
#define UACPI_FALSE false
typedef bool uacpi_bool;

#define UACPI_NULL NULL

typedef uintptr_t uacpi_virt_addr;
typedef size_t uacpi_size;

typedef va_list uacpi_va_list;

typedef char uacpi_char;

#endif
