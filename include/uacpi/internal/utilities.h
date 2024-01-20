#pragma once

#include <uacpi/types.h>
#include <uacpi/internal/log.h>
#include <uacpi/internal/stdlib.h>

static inline uacpi_phys_addr uacpi_truncate_phys_addr_with_warn(uacpi_u64 large_addr)
{
    if (sizeof(uacpi_phys_addr) < 8 && large_addr > 0xFFFFFFFF) {
        uacpi_warn(
            "truncating a physical address 0x%016llX outside of address space\n",
            large_addr
        );
    }

    return (uacpi_phys_addr)large_addr;
}

uacpi_status uacpi_verify_table_checksum_with_warn(void*, uacpi_size);
uacpi_status uacpi_check_tbl_signature_with_warn(void*, const char *expect);

#define UACPI_PTR_TO_VIRT_ADDR(ptr)   ((uacpi_virt_addr)(ptr))
#define UACPI_VIRT_ADDR_TO_PTR(vaddr) ((void*)(vaddr))

/*
 * Target buffer must have a length of at least 8 bytes.
 */
void uacpi_eisa_id_to_string(uacpi_u32, uacpi_char *out_string);

uacpi_status uacpi_eval_hid(uacpi_namespace_node*, uacpi_char **out_hid);
