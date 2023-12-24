#pragma once

#include <uacpi/internal/context.h>
#include <uacpi/types.h>
#include <uacpi/status.h>

enum uacpi_table_type {
    UACPI_TABLE_TYPE_FADT = 0,
    UACPI_TABLE_TYPE_DSDT = 1,
    UACPI_TABLE_TYPE_SSDT = 2,
    UACPI_TABLE_TYPE_OEM = 0xFF
};

// FADT + DSDT have a hardcoded index in the array
#define UACPI_BASE_TABLE_COUNT 2

uacpi_status uacpi_table_append(uacpi_phys_addr addr,
                                struct uacpi_table **out_table);

uacpi_status
uacpi_table_append_mapped(uacpi_virt_addr virt_addr,
                          struct uacpi_table **out_table);

uacpi_status
uacpi_table_find_by_type(enum uacpi_table_type type,
                         struct uacpi_table **out_table);


uacpi_status
uacpi_table_find_by_signature(uacpi_object_name signature,
                              struct uacpi_table **out_table);

uacpi_status
uacpi_table_find_next_with_same_signature(struct uacpi_table **in_out_table);
