#pragma once

#include <uacpi/acpi.h>
#include <uacpi/types.h>
#include <uacpi/uacpi.h>
#include <uacpi/internal/dynamic_array.h>
#include <uacpi/internal/shareable.h>

struct uacpi_runtime_context {
    /*
     * A local copy of FADT that has been verified & converted to most optimal
     * format for faster access to the registers.
     */
    struct acpi_fadt fadt;

    /*
     * A cached pointer to FACS so that we don't have to look it up in interrupt
     * contexts as we can't take mutexes.
     */
    struct acpi_facs *facs;

    /*
     * pm1{a,b}_evt_blk split into two registers for convenience
     */
    struct acpi_gas pm1a_status_blk;
    struct acpi_gas pm1b_status_blk;
    struct acpi_gas pm1a_enable_blk;
    struct acpi_gas pm1b_enable_blk;

#define UACPI_SLEEP_TYP_INVALID 0xFF
    uacpi_u8 last_sleep_typ_a;
    uacpi_u8 last_sleep_typ_b;

    uacpi_u8 s0_sleep_typ_a;
    uacpi_u8 s0_sleep_typ_b;

    /*
     * This is a per-table value but we mimic the NT implementation:
     * treat all other definition blocks as if they were the same revision
     * as DSDT.
     */
    uacpi_bool is_rev1;

#if UACPI_REDUCED_HARDWARE == 0
    uacpi_bool is_hardware_reduced;
    uacpi_bool has_global_lock;
    uacpi_handle sci_handle;
#endif

#define UACPI_INIT_LEVEL_EARLY 0
#define UACPI_INIT_LEVEL_TABLES_LOADED 1
#define UACPI_INIT_LEVEL_NAMESPACE_LOADED 2
#define UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED 3
    uacpi_u8 init_level;

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

static inline uacpi_bool uacpi_is_hardware_reduced(void)
{
#if UACPI_REDUCED_HARDWARE == 0
    return g_uacpi_rt_ctx.is_hardware_reduced;
#else
    return UACPI_TRUE;
#endif
}
