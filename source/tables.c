#include <uacpi/internal/tables.h>
#include <uacpi/internal/utilities.h>

static uacpi_object_name fadt_signature = {
    .text = { ACPI_FADT_SIGNATURE },
};

static uacpi_object_name dsdt_signature = {
    .text = { ACPI_DSDT_SIGNATURE },
};

DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(table_array, struct uacpi_table,)

// TODO: thread safety/locking
static uacpi_status table_do_alloc_slot(struct uacpi_table **out_table)
{
    *out_table = table_array_calloc(&g_uacpi_rt_ctx.tables);
    if (*out_table == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    return UACPI_STATUS_OK;
}

static struct uacpi_table*
get_table_for_type(enum uacpi_table_type type)
{
    switch (type) {
    case UACPI_TABLE_TYPE_FADT:
    case UACPI_TABLE_TYPE_DSDT:
        return table_array_at(&g_uacpi_rt_ctx.tables, type);
    default:
        break;
    }

    return UACPI_NULL;
}

static struct uacpi_table*
get_table_for_signature(uacpi_object_name signature)
{
    enum uacpi_table_type type = UACPI_TABLE_TYPE_OEM;

    if (signature.id == fadt_signature.id)
        type = UACPI_TABLE_TYPE_FADT;
    if (signature.id == dsdt_signature.id)
        type = UACPI_TABLE_TYPE_DSDT;

    return get_table_for_type(type);
}

static uacpi_status
table_alloc_slot_for_signature(uacpi_object_name signature,
                               struct uacpi_table **out_table)
{
    *out_table = get_table_for_signature(signature);
    if (*out_table)
        return UACPI_STATUS_OK;

    return table_do_alloc_slot(out_table);
}

static uacpi_status
get_external_table_signature_and_length(uacpi_phys_addr phys_addr,
                                        uacpi_object_name *out_sign,
                                        uacpi_u32 *out_len)
{
    struct acpi_sdt_hdr *hdr;

    hdr = uacpi_kernel_map(phys_addr, sizeof(struct acpi_sdt_hdr));
    if (!hdr)
        return UACPI_STATUS_MAPPING_FAILED;

    uacpi_memcpy(out_sign, hdr->signature, sizeof(hdr->signature));
    *out_len = hdr->length;

    uacpi_kernel_unmap(hdr, sizeof(struct acpi_sdt_hdr));

    if (*out_len == 0)
        return UACPI_STATUS_INVALID_TABLE_LENGTH;

    return UACPI_STATUS_OK;
}

static uacpi_status map_table(struct uacpi_table *table)
{
    void *virt_table;

    if (table->flags & UACPI_TABLE_MAPPED)
        return UACPI_STATUS_OK;

    virt_table = uacpi_kernel_map(table->phys_addr, table->length);
    if (!virt_table)
        return UACPI_STATUS_MAPPING_FAILED;

    table->flags |= UACPI_TABLE_MAPPED;
    table->virt_addr = UACPI_PTR_TO_VIRT_ADDR(virt_table);
    return UACPI_STATUS_OK;
}

uacpi_status
uacpi_table_append(uacpi_phys_addr addr, struct uacpi_table **out_table)
{
    uacpi_object_name signature;
    uacpi_u32 length;
    struct uacpi_table *table;
    uacpi_status ret;

    ret = get_external_table_signature_and_length(addr, &signature, &length);
    if (uacpi_unlikely_error(ret))
        return ret;

    ret = table_alloc_slot_for_signature(signature, &table);
    if (uacpi_unlikely_error(ret))
        return ret;

    table->phys_addr = addr;
    table->length = length;
    table->signature = signature;

    ret = map_table(table);
    if (uacpi_unlikely_error(ret))
        goto out_bad_table;

    ret = uacpi_verify_table_checksum_with_warn(
        UACPI_VIRT_ADDR_TO_PTR(table->virt_addr),
        table->length
    );
    if (uacpi_unlikely_error(ret))
        goto out_bad_table;

    if (table->signature.id == fadt_signature.id) {
        struct acpi_fadt *fadt = UACPI_VIRT_ADDR_TO_PTR(table->virt_addr);
        ret = uacpi_table_append(fadt->x_dsdt ? fadt->x_dsdt : fadt->dsdt,
                                 UACPI_NULL);
    }

    if (table->signature.id == dsdt_signature.id) {
        struct acpi_dsdt *dsdt = UACPI_VIRT_ADDR_TO_PTR(table->virt_addr);
        g_uacpi_rt_ctx.is_rev1 = dsdt->hdr.revision < 2;
    }

    if (out_table != UACPI_NULL)
        *out_table = table;

    table->flags |= UACPI_TABLE_VALID;

out_bad_table:
    return ret;
}

uacpi_status
uacpi_table_append_mapped(uacpi_virt_addr virt_addr, struct uacpi_table **out_table)
{
    uacpi_object_name signature;
    struct uacpi_table *table;
    struct acpi_sdt_hdr *hdr;
    uacpi_status ret;

    hdr = UACPI_VIRT_ADDR_TO_PTR(virt_addr);
    if (hdr->length == 0)
        return UACPI_STATUS_INVALID_TABLE_LENGTH;

    uacpi_memcpy(&signature, hdr->signature, sizeof(hdr->signature));

    ret = table_alloc_slot_for_signature(signature, &table);
    if (uacpi_unlikely_error(ret))
        return ret;

    table->virt_addr = virt_addr;
    table->flags = UACPI_TABLE_MAPPED;
    table->length = hdr->length;

    ret = uacpi_verify_table_checksum_with_warn(hdr, table->length);
    if (uacpi_unlikely_error(ret))
        return ret;

    if (out_table != UACPI_NULL)
        *out_table = table;

    table->flags |= UACPI_TABLE_VALID;
    return UACPI_STATUS_OK;
}

struct table_search_spec {
    uacpi_object_name signature;
    bool has_signature;

    enum uacpi_table_type type;
    bool has_type;

    uacpi_size base_idx;
};

static uacpi_status do_search(struct table_search_spec *spec,
                              struct uacpi_table **out_table)
{
    uacpi_size idx;
    struct uacpi_table *found_table = UACPI_NULL;

    for (idx = spec->base_idx;
        idx < table_array_size(&g_uacpi_rt_ctx.tables); ++idx
    ) {
        uacpi_size real_idx = idx + UACPI_BASE_TABLE_COUNT;
        struct uacpi_table *table;

        table = table_array_at(&g_uacpi_rt_ctx.tables, real_idx);

        if (!(table->flags & UACPI_TABLE_VALID))
            continue;

        if (spec->has_signature &&
            spec->signature.id == table->signature.id) {
            found_table = table;
            break;
        }

        if (spec->has_type && spec->type == table->type) {
            found_table = table;
            break;
        }
    }

    if (found_table == UACPI_NULL)
        return UACPI_STATUS_NOT_FOUND;

    *out_table = found_table;
    return UACPI_STATUS_OK;
}

uacpi_status
uacpi_table_find_by_type(enum uacpi_table_type type,
                           struct uacpi_table **out_table)
{
    *out_table = get_table_for_type(type);

    if (*out_table == UACPI_NULL) {
        struct table_search_spec spec = {
            .type = type,
            .has_type = true,
            .base_idx = 0
        };

        return do_search(&spec, out_table);
    }

    return UACPI_STATUS_OK;
}

uacpi_status
uacpi_table_find_by_signature(uacpi_object_name signature,
                              struct uacpi_table **out_table)
{
    *out_table = get_table_for_signature(signature);

    if (*out_table == UACPI_NULL) {
        struct table_search_spec spec = {
            .signature = signature,
            .has_signature = true,
            .base_idx = 0
        };

        return do_search(&spec, out_table);
    }

    return UACPI_STATUS_OK;
}

static uacpi_size table_array_index_of(
    struct table_array *arr, struct uacpi_table *tbl
)
{
    struct uacpi_table *end;
    uacpi_size inline_cap;

    inline_cap = table_array_inline_capacity(arr);
    end = arr->inline_storage + inline_cap;
    if (tbl < end)
        return tbl - arr->inline_storage;

    end = arr->dynamic_storage + (arr->size_including_inline - inline_cap);
    if (tbl > end)
        return 0;

    return tbl - arr->dynamic_storage;
}


uacpi_status
uacpi_table_next_with_same_signature(struct uacpi_table **in_out_table)
{
    struct table_search_spec spec = {
        .signature = (*in_out_table)->signature,
        .has_signature = true
    };

    // Tables like FADT, DSDT etc. are always unique, we can fail this right away
    if (get_table_for_signature(spec.signature))
        return UACPI_STATUS_NOT_FOUND;

    spec.base_idx = table_array_index_of(&g_uacpi_rt_ctx.tables, *in_out_table);
    if (spec.base_idx == 0)
        return UACPI_STATUS_INVALID_ARGUMENT;

    spec.base_idx -= UACPI_BASE_TABLE_COUNT;
    return do_search(&spec, in_out_table);
}
