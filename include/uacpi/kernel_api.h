#pragma once

#include <uacpi/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Map a SystemIO address at [base, base + len) and return a kernel-implemented
 * handle that can be used for reading and writing the IO range.
 */
uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
);
void uacpi_kernel_io_unmap(uacpi_handle handle);

/*
 * Read/Write the IO range mapped via uacpi_kernel_io_map
 * at a 0-based 'offset' within the range.
 */
uacpi_status uacpi_kernel_io_read(
    uacpi_handle, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 *value
);
uacpi_status uacpi_kernel_io_write(
    uacpi_handle, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 value
);

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

/*
 * Spin for N microseconds.
 */
void uacpi_kernel_stall(uacpi_u8 usec);

/*
 * Sleep for N milliseconds.
 */
void uacpi_kernel_sleep(uacpi_u64 msec);

/*
 * Create/free an opaque non-recursive kernel mutex object.
 */
uacpi_handle uacpi_kernel_create_mutex(void);
void uacpi_kernel_free_mutex(uacpi_handle);

/*
 * Create/free an opaque kernel (semaphore-like) event object.
 */
uacpi_handle uacpi_kernel_create_event(void);
void uacpi_kernel_free_event(uacpi_handle);

/*
 * Try to acquire the mutex with a millisecond timeout.
 * A timeout value of 0xFFFF implies infinite wait.
 */
uacpi_bool uacpi_kernel_acquire_mutex(uacpi_handle, uacpi_u16);
void uacpi_kernel_release_mutex(uacpi_handle);

/*
 * Try to wait for an event (counter > 0) with a millisecond timeout.
 * A timeout value of 0xFFFF implies infinite wait.
 *
 * The internal counter is decremented by 1 if wait was successful.
 *
 * A successful wait is indicated by returning UACPI_TRUE.
 */
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16);

/*
 * Signal the event object by incrementing its internal counter by 1.
 */
void uacpi_kernel_signal_event(uacpi_handle);

/*
 * Reset the event counter to 0.
 */
void uacpi_kernel_reset_event(uacpi_handle);

#ifdef __cplusplus
}
#endif