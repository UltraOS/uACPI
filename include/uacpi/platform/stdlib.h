#pragma once

/*
 * Platform-specific standard library functions go here. This is the default
 * placeholder using libc/posix headers.
 */

#ifdef UACPI_OVERRIDE_STDLIB
#include "uacpi_stdlib.h"
#else

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>

#define UACPI_PRIx64 PRIx64
#define UACPI_PRIX64 PRIX64
#define UACPI_PRIu64 PRIu64

#define uacpi_memcpy memcpy
#define uacpi_memset memset
#define uacpi_memcmp memcmp
#define uacpi_strncmp strncmp
#define uacpi_strcmp strcmp
#define uacpi_memmove memmove
#define uacpi_strnlen strnlen
#define uacpi_strlen strlen
#define uacpi_snprintf snprintf

#define uacpi_offsetof offsetof

#endif
