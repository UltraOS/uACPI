#pragma once

#include <uacpi/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len);
void uacpi_kernel_unmap(void *addr, uacpi_size len);

void *uacpi_kernel_alloc(uacpi_size size);
void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size);
void uacpi_kernel_free(void *mem);

enum uacpi_log_level {
    UACPI_LOG_TRACE = 4,
    UACPI_LOG_INFO  = 3,
    UACPI_LOG_WARN  = 2,
    UACPI_LOG_ERROR = 1,
    UACPI_LOG_FATAL = 0
};

void uacpi_kernel_log(enum uacpi_log_level, const char*, ...);
void uacpi_kernel_vlog(enum uacpi_log_level, const char*, uacpi_va_list);

/*
 * Returns the number of 100 nanosecond ticks, strictly monotonic.
 */
uacpi_u64 uacpi_kernel_get_ticks(void);

#ifdef __cplusplus
}
#endif