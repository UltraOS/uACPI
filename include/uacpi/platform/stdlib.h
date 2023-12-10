#pragma once

/*
 * Platform-specific standard library functions go here. This is the default
 * placeholder using libc/posix headers.
 */

#include <string.h>
#include <stdlib.h>

#define uacpi_memcpy memcpy
#define uacpi_memset memset
#define uacpi_memcmp memcmp
#define uacpi_strnlen strnlen
#define uacpi_strtoull strtoull
