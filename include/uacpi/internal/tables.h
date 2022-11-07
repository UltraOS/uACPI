#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

enum uacpi_table_type {
    UACPI_TABLE_TYPE_FADT = 0,
    UACPI_TABLE_TYPE_DSDT = 1,
    UACPI_TABLE_TYPE_OEM = 0xFF
};

// FADT + DSDT have a hardcoded index in the array
#define UACPI_BASE_TABLE_COUNT 2

uacpi_status uacpi_table_append(uacpi_phys_addr addr);

uacpi_status
uacpi_table_append_acquire(uacpi_phys_addr addr, struct uacpi_table **out_table);

uacpi_status
uacpi_table_append_mapped(uacpi_virt_addr virt_addr,
                          struct uacpi_table **out_table);

uacpi_status
uacpi_table_acquire_by_type(enum uacpi_table_type type,
                            struct uacpi_table **out_table);


uacpi_status
uacpi_table_acquire_by_signature(union uacpi_table_signature signature,
                                 struct uacpi_table **out_table);

uacpi_status
uacpi_table_acquire_next_with_same_signature(struct uacpi_table **in_out_table);

uacpi_status
uacpi_table_release(struct uacpi_table *out_table);
