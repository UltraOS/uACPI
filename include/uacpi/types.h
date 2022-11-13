#pragma once
#include <uacpi/platform/types.h>
#include <uacpi/platform/compiler.h>

#if UACPI_POINTER_SIZE == 4 && defined(UACPI_PHYS_ADDR_IS_32BITS)
typedef uint32_t uacpi_phys_addr;
typedef uint32_t uacpi_io_addr;
#else
typedef uint64_t uacpi_phys_addr;
typedef uint64_t uacpi_io_addr;
#endif

