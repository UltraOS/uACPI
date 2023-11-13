#pragma once

#include <uacpi/platform/stdlib.h>

#define uacpi_memzero(ptr, size) uacpi_memset(ptr, 0, size)

#define UACPI_COMPARE(x, y, op) ((x) op (y) ? (x) : (y))
#define UACPI_MIN(x, y) UACPI_COMPARE(x, y, <)
#define UACPI_MAX(x, y) UACPI_COMPARE(x, y, >)
