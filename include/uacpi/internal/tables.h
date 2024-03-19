#pragma once

#include <uacpi/internal/context.h>
#include <uacpi/types.h>
#include <uacpi/status.h>

enum uacpi_table_type {
    UACPI_TABLE_TYPE_FADT = 0,
    UACPI_TABLE_TYPE_DSDT = 1,
    UACPI_TABLE_TYPE_SSDT = 2,
    UACPI_TABLE_TYPE_INVALID = 0xFFFF,
};

// FADT + DSDT have a hardcoded index in the array
#define UACPI_BASE_TABLE_COUNT 2

struct uacpi_table_identifiers {
    uacpi_object_name signature;

    // if oemid[0] == 0 this field is ignored
    char oemid[6];

    // if oem_table_id[0] == 0 this field is ignored
    char oem_table_id[8];
};

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

uacpi_status
uacpi_table_find(struct uacpi_table_identifiers *id,
                 struct uacpi_table **out_table);

/*
 * Returns the pointer to a sanitized internal version of FADT.
 *
 * The revision is guaranteed to be correct. All of the registers are converted
 * to GAS format. Fields that might contain garbage are cleared.
 */
uacpi_status uacpi_table_fadt(struct acpi_fadt**);
