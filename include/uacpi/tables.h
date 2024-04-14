#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
* Table is valid and may be used
*/
#define UACPI_TABLE_VALID (1 << 0)

/*
 * Table is already mapped and 'virt_addr' may be used to read it
 */
#define UACPI_TABLE_MAPPED (1 << 1)

/*
 * (Only relevant for definition blocks)
 * The table has already been executed by the interpreter.
 */
#define UACPI_TABLE_LOADED (1 << 2)

typedef struct uacpi_table {
    struct uacpi_shareable shareable;
    uacpi_object_name signature;
    uacpi_phys_addr phys_addr;
    union {
        uacpi_virt_addr virt_addr;
        struct acpi_sdt_hdr *hdr;
    };
    uacpi_u32 length;
    uacpi_u8 flags;
} uacpi_table;

typedef enum uacpi_table_type {
    UACPI_TABLE_TYPE_FADT = 0,
    UACPI_TABLE_TYPE_DSDT = 1,
    UACPI_TABLE_TYPE_INVALID = 0xFFFF,
} uacpi_table_type;

typedef struct uacpi_table_identifiers {
    uacpi_object_name signature;

    // if oemid[0] == 0 this field is ignored
    char oemid[6];

    // if oem_table_id[0] == 0 this field is ignored
    char oem_table_id[8];
} uacpi_table_identifiers;

uacpi_status
uacpi_table_find_by_type(uacpi_table_type type,
                         uacpi_table **out_table);


uacpi_status
uacpi_table_find_by_signature(uacpi_object_name signature,
                              uacpi_table **out_table);

uacpi_status
uacpi_table_find_next_with_same_signature(uacpi_table **in_out_table);

uacpi_status
uacpi_table_find(uacpi_table_identifiers *id, uacpi_table **out_table);

/*
 * Returns the pointer to a sanitized internal version of FADT.
 *
 * The revision is guaranteed to be correct. All of the registers are converted
 * to GAS format. Fields that might contain garbage are cleared.
 */
uacpi_status uacpi_table_fadt(struct acpi_fadt**);

#ifdef __cplusplus
}
#endif
