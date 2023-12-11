#pragma once

#include <uacpi/internal/types.h>
#include <uacpi/platform/stdlib.h>

#define uacpi_memzero(ptr, size) uacpi_memset(ptr, 0, size)

#define UACPI_COMPARE(x, y, op) ((x) op (y) ? (x) : (y))
#define UACPI_MIN(x, y) UACPI_COMPARE(x, y, <)
#define UACPI_MAX(x, y) UACPI_COMPARE(x, y, >)

#define UACPI_ALIGN_UP_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define UACPI_ALIGN_UP(x, val, type) UACPI_ALIGN_UP_MASK(x, (type)(val) - 1)

#define UACPI_ALIGN_DOWN_MASK(x, mask) ((x) & ~(mask))
#define UACPI_ALIGN_DOWN(x, val, type) UACPI_ALIGN_DOWN_MASK(x, (type)(val) - 1)

#define UACPI_IS_ALIGNED_MASK(x, mask) (((x) & (mask)) == 0)
#define UACPI_IS_ALIGNED(x, val, type) UACPI_IS_ALIGNED_MASK(x, (type)(val) - 1)

void uacpi_memcpy_zerout(void *dst, const void *src,
                         uacpi_size dst_size, uacpi_size src_size);

// Returns the one-based bit location of LSb or 0
uacpi_u8 uacpi_bit_scan_forward(uacpi_u64);
