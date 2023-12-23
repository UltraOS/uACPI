#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>
#include <uacpi/kernel_api.h>
#include <uacpi/namespace.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bad table checksum should be considered a fatal error
 * (table load is fully aborted in this case)
 */
#define UACPI_PARAM_BAD_CSUM_FATAL (1 << 0)

/*
 * Bad table header should be considered a fatal error
 * (table load is fully aborted in this case)
 */
#define UACPI_PARAM_BAD_TBL_HDR_FATAL (1 << 1)

/*
 * Force uACPI to use RSDT even for later revisions
 */
#define UACPI_PARAM_BAD_XSDT (1 << 2)

struct uacpi_params {
    enum uacpi_log_level log_level;
    uint64_t flags;
};

struct uacpi_init_params {
    uacpi_phys_addr rsdp;
    struct uacpi_params rt_params;
};

/*
 * Initializes the uACPI subsystem, iterates & records all relevant RSDT/XSDT
 * tables.
 */
uacpi_status uacpi_initialize(struct uacpi_init_params*);

/*
 * Parses & executes all of the DSDT/SSDT tables.
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
                        uacpi_args *args, uacpi_object **ret);

#ifdef __cplusplus
}
#endif
