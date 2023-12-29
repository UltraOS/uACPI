#pragma once

/*
 * Platform-specific standard library functions go here. This is the default
 * placeholder using libc/posix headers.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#define UACPI_PRIx64 PRIx64
#define UACPI_PRIX64 PRIX64
#define UACPI_PRIu64 PRIu64

#define uacpi_memcpy memcpy
#define uacpi_memset memset
#define uacpi_memcmp memcmp
#define uacpi_memmove memmove
#define uacpi_strnlen strnlen
#define uacpi_strlen strlen
#define uacpi_strtoull strtoull
#define uacpi_snprintf snprintf
