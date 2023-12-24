#pragma once

#include <uacpi/acpi.h>
#include <uacpi/types.h>
#include <uacpi/uacpi.h>
#include <uacpi/internal/dynamic_array.h>
#include <uacpi/internal/shareable.h>

 /*
  * Table is valid and may be used
  */
#define UACPI_TABLE_VALID (1 << 0)

/*
 * Table is already mapped and 'virt_addr' may be used to read it
 */
#define UACPI_TABLE_MAPPED (1 << 1)

struct uacpi_table {
    struct uacpi_shareable shareable;
    uacpi_object_name signature;
    uacpi_phys_addr phys_addr;
    union {
        uacpi_virt_addr virt_addr;
        struct acpi_sdt_hdr *hdr;
    };
    uacpi_u32 length;
    uacpi_u8 flags;
};

#define UACPI_STATIC_TABLE_ARRAY_LEN 16

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(
    table_array, struct uacpi_table, UACPI_STATIC_TABLE_ARRAY_LEN
)

struct uacpi_runtime_context {
    struct table_array tables;

    /*
     * This is a per-table value but we mimic the NT implementation:
     * treat all other definition blocks as if they were the same revision
     * as DSDT.
     */
    uacpi_bool is_rev1;

    struct uacpi_params params;
};

extern struct uacpi_runtime_context g_uacpi_rt_ctx;

static inline uacpi_bool uacpi_rt_params_check(uacpi_u64 flag)
{
    return (g_uacpi_rt_ctx.params.flags & flag) == flag;
}

static inline uacpi_bool uacpi_rt_should_log(enum uacpi_log_level lvl)
{
    return lvl <= g_uacpi_rt_ctx.params.log_level;
}
