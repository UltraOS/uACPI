#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>

#include <uacpi/internal/log.h>
#include <uacpi/internal/context.h>
#include <uacpi/internal/utilities.h>
#include <uacpi/internal/tables.h>

struct uacpi_runtime_context g_uacpi_rt_ctx = { 0 };

const char *uacpi_status_to_string(uacpi_status st)
{
    switch (st) {
    case UACPI_STATUS_OK:
        return "no error";
    case UACPI_STATUS_MAPPING_FAILED:
        return "failed to map memory";
    case UACPI_STATUS_OUT_OF_MEMORY:
        return "out of memory";
    case UACPI_STATUS_BAD_CHECKSUM:
        return "bad table checksum";
    case UACPI_STATUS_INVALID_SIGNATURE:
        return "invalid table signature";
    case UACPI_STATUS_NOT_FOUND:
        return "not found";
    case UACPI_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case UACPI_STATUS_OUT_OF_BOUNDS:
        return "out of bounds";
    case UACPI_STATUS_BAD_BYTECODE:
        return "bad bytecode";
    case UACPI_STATUS_UNIMPLEMENTED:
        return "unimplemented";
    default:
        return "<invalid status>";
    }
}

UACPI_PACKED(struct uacpi_rxsdt {
    struct acpi_sdt_hdr hdr;
    uacpi_u8 ptr_bytes[];
})

static uacpi_status initialize_from_rxsdt(uacpi_phys_addr rxsdt_addr,
                                          uacpi_size entry_size)
{
    struct uacpi_rxsdt *rxsdt;
    uacpi_size i, entry_bytes, map_len = sizeof(*rxsdt);
    uacpi_phys_addr entry_addr;
    uacpi_status ret;

    rxsdt = uacpi_kernel_map(rxsdt_addr, map_len);
    if (rxsdt == UACPI_NULL)
        return UACPI_STATUS_MAPPING_FAILED;

    ret = uacpi_check_tbl_signature_with_warn(rxsdt,
        entry_size == 8 ? ACPI_XSDT_SIGNATURE : ACPI_RSDT_SIGNATURE);
    if (uacpi_unlikely_error(ret))
        goto error_out;

    map_len = rxsdt->hdr.length;
    uacpi_kernel_unmap(rxsdt, sizeof(*rxsdt));

    if (uacpi_unlikely(map_len < (sizeof(*rxsdt) + entry_size)))
        return UACPI_STATUS_INVALID_TABLE_LENGTH;

    // Make sure length is aligned to entry size so we don't OOB
    entry_bytes = map_len - sizeof(*rxsdt);
    entry_bytes &= ~(entry_size - 1);

    rxsdt = uacpi_kernel_map(rxsdt_addr, map_len);
    if (uacpi_unlikely(rxsdt == UACPI_NULL))
        return UACPI_STATUS_MAPPING_FAILED;

    ret = uacpi_verify_table_checksum_with_warn(rxsdt, map_len);
    if (uacpi_unlikely_error(ret))
        goto error_out;

    for (i = 0; i < entry_bytes; i += entry_size) {
        uacpi_u64 entry_phys_addr_large = 0;
        uacpi_memcpy(&entry_phys_addr_large, &rxsdt->ptr_bytes[i], entry_size);

        if (!entry_phys_addr_large)
            continue;

        entry_addr = uacpi_truncate_phys_addr_with_warn(entry_phys_addr_large);
        ret = uacpi_table_append(entry_addr);
        if (uacpi_unlikely_error(ret))
            return ret;
    }

    ret = UACPI_STATUS_OK;

error_out:
    uacpi_kernel_unmap(rxsdt, map_len);
    return ret;
}

uacpi_status uacpi_initialize(struct uacpi_init_params *params)
{
    struct acpi_rsdp *rsdp;
    uacpi_phys_addr rxsdt;
    uacpi_size rxsdt_entry_size;

    uacpi_memcpy(&g_uacpi_rt_ctx.params, &params->rt_params,
                 sizeof(params->rt_params));

    rsdp = uacpi_kernel_map(params->rsdp, sizeof(struct acpi_rsdp));
    if (rsdp == UACPI_NULL)
        return UACPI_STATUS_MAPPING_FAILED;

    if (rsdp->revision > 0 &&
        !uacpi_rt_params_check(UACPI_PARAM_BAD_XSDT))
    {
        rxsdt = uacpi_truncate_phys_addr_with_warn(rsdp->xsdt_addr);
        rxsdt_entry_size = 8;
    } else {
        rxsdt = (uacpi_phys_addr)rsdp->rsdt_addr;
        rxsdt_entry_size = 4;
    }

    uacpi_kernel_unmap(rsdp, sizeof(struct acpi_rsdp));

    return initialize_from_rxsdt(rxsdt, rxsdt_entry_size);
}

uacpi_status uacpi_namespace_load(void)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_namespace_initialize(void)
{
    return UACPI_STATUS_OK;
}

uacpi_status
uacpi_eval(uacpi_handle *root_handle, const uacpi_char *path, uacpi_args *args,
           uacpi_retval *ret)
{
    if (!root_handle && !path)
        return UACPI_STATUS_INVALID_ARGUMENT;

    UACPI_UNUSED(ret);
    UACPI_UNUSED(args);
    return UACPI_STATUS_OK;
}
