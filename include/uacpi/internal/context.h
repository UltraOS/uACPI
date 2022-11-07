#pragma once

#include <uacpi/acpi.h>
#include <uacpi/types.h>
#include <uacpi/uacpi.h>

 /*
  * Table is invalid and should be ignored
  */
#define UACPI_TABLE_INVALID      (1 << 0)

/*
 * Table should not be unmapped when there are no references to it.
 * Possibly of internal origin without a known physical address.
 */
#define UACPI_TABLE_NO_UNMAP     (1 << 1)

union uacpi_table_signature {
    char as_chars[4];
    uacpi_u32 as_u32;
};

struct uacpi_table {
    union uacpi_table_signature signature;
    uacpi_u32 length;
    uacpi_phys_addr phys_addr;
    uacpi_virt_addr virt_addr;
    uacpi_u16 refs;
    uacpi_u16 flags;
    uacpi_u8 type;
};

#define UACPI_STATIC_TABLE_ARRAY_LEN 16

struct uacpi_runtime_context {
    struct uacpi_table tables[UACPI_STATIC_TABLE_ARRAY_LEN];
    struct uacpi_table *extra_tables;
    uacpi_size table_count;

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
