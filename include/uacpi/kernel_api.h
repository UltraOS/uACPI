#pragma once

#include <uacpi/types.h>
#include <uacpi/platform/arch_helpers.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Raw IO API, this is only used for accessing verified data from
 * "safe" code (aka not indirectly invoked by the AML interpreter),
 * e.g. programming FADT & FACS registers.
 * -------------------------------------------------------------------------
 */
uacpi_status uacpi_kernel_raw_memory_read(
    uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value
);
uacpi_status uacpi_kernel_raw_memory_write(
    uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 in_value
);

uacpi_status uacpi_kernel_raw_io_read(
    uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value
);
uacpi_status uacpi_kernel_raw_io_write(
    uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64 in_value
);
// -------------------------------------------------------------------------

uacpi_status uacpi_kernel_pci_read(
    uacpi_pci_address *address, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 *value
);
uacpi_status uacpi_kernel_pci_write(
    uacpi_pci_address *address, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 value
);

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
    UACPI_LOG_TRACE = 3,
    UACPI_LOG_INFO  = 2,
    UACPI_LOG_WARN  = 1,
    UACPI_LOG_ERROR = 0,
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
 *
 * This function may be used in interrupt contexts.
 */
void uacpi_kernel_signal_event(uacpi_handle);

/*
 * Reset the event counter to 0.
 */
void uacpi_kernel_reset_event(uacpi_handle);

/*
 * Handle a firmware request.
 *
 * Currently either a Breakpoint or Fatal operators.
 */
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*);

/*
 * Install an interrupt handler at 'irq', 'ctx' is passed to the provided
 * handler for every invocation.
 *
 * 'out_irq_handle' is set to a kernel-implemented value that can be used to
 * refer to this handler from other API.
 */
uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
);

/*
 * Uninstall an interrupt handler. 'irq_handle' is the value returned via
 * 'out_irq_handle' during installation.
 */
uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
);

/*
 * Create/free a kernel spinlock object.
 *
 * Unlike other types of locks, spinlocks may be used in interrupt contexts.
 */
uacpi_handle uacpi_kernel_create_spinlock(void);
void uacpi_kernel_free_spinlock(uacpi_handle);

/*
 * Lock/unlock helpers for spinlocks.
 *
 * These are expected to disable interrupts, returning the previous state of cpu
 * flags, that can be used to possibly re-enable interrupts if they were enabled
 * before.
 *
 * Note that lock is infalliable.
 */
uacpi_cpu_flags uacpi_kernel_spinlock_lock(uacpi_handle);
void uacpi_kernel_spinlock_unlock(uacpi_handle, uacpi_cpu_flags);

typedef enum uacpi_work_type {
    /*
     * Schedule a GPE handler method for execution.
     * This should be scheduled to run on CPU0 to avoid potential SMI-related
     * firmware bugs.
     */
    UACPI_WORK_GPE_EXECUTION,

    /*
     * Schedule a Notify(device) firmware request for execution.
     * This can run on any CPU.
     */
    UACPI_WORK_NOTIFICATION,
} uacpi_work_type;

typedef void (*uacpi_work_handler)(uacpi_handle);

/*
 * Schedules deferred work for execution.
 * Might be invoked from an interrupt context.
 */
uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
);

/*
 * Blocks until all scheduled work is complete and the work queue becomes empty.
 */
uacpi_status uacpi_kernel_wait_for_work_completion(void);

#ifdef __cplusplus
}
#endif
