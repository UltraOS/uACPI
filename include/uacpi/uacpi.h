#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>
#include <uacpi/kernel_api.h>
#include <uacpi/namespace.h>

#ifdef UACPI_REDUCED_HARDWARE
#define UACPI_MAKE_STUB_FOR_REDUCED_HARDWARE(fn, ret) \
    UACPI_NO_UNUSED_PARAMETER_WARNINGS_BEGIN          \
    static inline fn { return ret; }                \
    UACPI_NO_UNUSED_PARAMETER_WARNINGS_END

#define UACPI_STUB_IF_REDUCED_HARDWARE(fn) \
    UACPI_MAKE_STUB_FOR_REDUCED_HARDWARE(fn,)
#define UACPI_ALWAYS_ERROR_FOR_REDUCED_HARDWARE(fn) \
    UACPI_MAKE_STUB_FOR_REDUCED_HARDWARE(fn, UACPI_STATUS_COMPILED_OUT)
#define UACPI_ALWAYS_OK_FOR_REDUCED_HARDWARE(fn) \
    UACPI_MAKE_STUB_FOR_REDUCED_HARDWARE(fn, UACPI_STATUS_OK)
#else

#define UACPI_STUB_IF_REDUCED_HARDWARE(fn) fn;
#define UACPI_ALWAYS_ERROR_FOR_REDUCED_HARDWARE(fn) fn;
#define UACPI_ALWAYS_OK_FOR_REDUCED_HARDWARE(fn) fn;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uacpi_init_params {
    // Physical address of the RSDP structure.
    uacpi_phys_addr rsdp;

    // Initial log level, all logs above this level are discarded
    uacpi_log_level log_level;

/*
 * Bad table checksum should be considered a fatal error
 * (table load is fully aborted in this case)
 */
#define UACPI_FLAG_BAD_CSUM_FATAL (1 << 0)

/*
 * Unexpected table signature should be considered a fatal error
 * (table load is fully aborted in this case)
 */
#define UACPI_FLAG_BAD_TBL_SIGNATURE_FATAL (1 << 1)

/*
 * Force uACPI to use RSDT even for later revisions
 */
#define UACPI_FLAG_BAD_XSDT (1 << 2)

/*
 * If this is set, ACPI mode is not entered during the call to
 * uacpi_initialize. The caller is expected to enter it later at thier own
 * discretion by using uacpi_enter_acpi_mode().
 */
#define UACPI_FLAG_NO_ACPI_MODE (1 << 3)

/*
 * Don't create the \_OSI method when building the namespace.
 * Only enable this if you're certain that having this method breaks your AML
 * blob, a more atomic/granular interface management is available via osi.h
 */
#define UACPI_FLAG_NO_OSI (1 << 4)
    uacpi_u64 flags;
} uacpi_init_params;

/*
 * Initializes the uACPI subsystem, iterates & records all relevant RSDT/XSDT
 * tables. Enters ACPI mode.
 */
uacpi_status uacpi_initialize(const struct uacpi_init_params*);

/*
 * Parses & executes all of the DSDT/SSDT tables.
 * Initializes the event subsystem.
 */
uacpi_status uacpi_namespace_load(void);

/*
 * Initializes all the necessary objects in the namespaces by calling
 * _STA/_INI etc.
 */
uacpi_status uacpi_namespace_initialize(void);

/*
 * Evaluate an object within the namespace and get back its value.
 * Either root or path must be valid.
 * A value of NULL for 'parent' implies uacpi_namespace_root() relative
 * lookups, unless 'path' is already absolute.
 */
uacpi_status uacpi_eval(uacpi_namespace_node *parent, const uacpi_char *path,
                        const uacpi_args *args, uacpi_object **ret);

/*
 * Same as uacpi_eval, but the return value type is validated against
 * the 'ret_mask'. UACPI_STATUS_TYPE_MISMATCH is returned on error.
 */
uacpi_status uacpi_eval_typed(
    uacpi_namespace_node *parent, const uacpi_char *path,
    const uacpi_args *args, uacpi_u32 ret_mask, uacpi_object **ret
);

/*
 * A shorthand for uacpi_eval_typed with UACPI_OBJECT_INTEGER_BIT.
 */
uacpi_status uacpi_eval_integer(
    uacpi_namespace_node *parent, const uacpi_char *path,
    const uacpi_args *args, uacpi_u64 *out_value
);

/*
 * Helpers for entering & leaving ACPI mode. Note that ACPI mode is entered
 * automatically during the call to uacpi_initialize().
 */
UACPI_ALWAYS_OK_FOR_REDUCED_HARDWARE(
    uacpi_status uacpi_enter_acpi_mode(void)
)
UACPI_ALWAYS_ERROR_FOR_REDUCED_HARDWARE(
    uacpi_status uacpi_leave_acpi_mode(void)
)

/*
 * Attempt to acquire the global lock for 'timeout' milliseconds.
 * 0xFFFF implies infinite wait.
 *
 * On success, 'out_seq' is set to a unique sequence number for the current
 * acquire transaction. This number is used for validation during release.
 */
uacpi_status uacpi_acquire_global_lock(uacpi_u16 timeout, uacpi_u32 *out_seq);
uacpi_status uacpi_release_global_lock(uacpi_u32 seq);

#ifdef __cplusplus
}
#endif
