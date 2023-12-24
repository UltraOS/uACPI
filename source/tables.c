#include <uacpi/internal/tables.h>
#include <uacpi/internal/utilities.h>

static union uacpi_table_signature fadt_signature = {
    .as_chars = { ACPI_FADT_SIGNATURE },
};

static union uacpi_table_signature dsdt_signature = {
    .as_chars = { ACPI_DSDT_SIGNATURE },
};

#define UACPI_TABLE_BUGGED_REFCOUNT 0xFFFF

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
get_table_for_signature(union uacpi_table_signature signature)
{
    enum uacpi_table_type type = UACPI_TABLE_TYPE_OEM;

    if (signature.as_u32 == fadt_signature.as_u32)
        type = UACPI_TABLE_TYPE_FADT;
    if (signature.as_u32 == dsdt_signature.as_u32)
        type = UACPI_TABLE_TYPE_DSDT;

    return get_table_for_type(type);
}

static uacpi_status
table_alloc_slot_for_signature(union uacpi_table_signature signature,
                               struct uacpi_table **out_table)
{
    *out_table = get_table_for_signature(signature);
    if (*out_table)
        return UACPI_STATUS_OK;

    return table_do_alloc_slot(out_table);
}

static uacpi_u16 table_incref(struct uacpi_table *table)
{
    if (uacpi_likely(table->refs != UACPI_TABLE_BUGGED_REFCOUNT))
        table->refs++;

    return table->refs;
}

static uacpi_u16 table_decref(struct uacpi_table *table)
{
    if (uacpi_unlikely(table->refs == 0)) {
        uacpi_kernel_log(
            UACPI_LOG_WARN,
            "BUG: table '%.4s' refcount underflow, keeping a permanent mapping",
            table->signature.as_chars
        );

        table->flags |= UACPI_TABLE_NO_UNMAP;
        table->refs = UACPI_TABLE_BUGGED_REFCOUNT;
    }

    if (uacpi_likely(table->refs != UACPI_TABLE_BUGGED_REFCOUNT))
        table->refs--;

    return table->refs;
}

static uacpi_status do_acquire_table(struct uacpi_table *table)
{
    if (!table->refs && !(table->flags & UACPI_TABLE_NO_UNMAP)) {
        void *virt_table;

        virt_table = uacpi_kernel_map(table->phys_addr, table->length);
        if (!virt_table)
            return UACPI_STATUS_MAPPING_FAILED;

        table->virt_addr = UACPI_PTR_TO_VIRT_ADDR(virt_table);
    }

    table_incref(table);
    return UACPI_STATUS_OK;
}

static uacpi_status do_release_table(struct uacpi_table *table)
{
    uacpi_size refs = table_decref(table);

    if (refs == 0 && !(table->flags & UACPI_TABLE_NO_UNMAP)) {
        uacpi_kernel_unmap(UACPI_VIRT_ADDR_TO_PTR(table->virt_addr),
                           table->length);
        table->virt_addr = 0;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status
get_external_table_signature_and_length(uacpi_phys_addr phys_addr,
                                        union uacpi_table_signature *out_sign,
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

static uacpi_status
uacpi_table_do_append(uacpi_phys_addr addr, struct uacpi_table **out_table)
{
    union uacpi_table_signature signature;
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
    table->virt_addr = 0;
    table->refs = 0;
    table->flags = 0;
    table->length = length;
    table->signature = signature;

    ret = do_acquire_table(table);
    if (uacpi_unlikely_error(ret))
        goto out_bad_table;

    ret = uacpi_verify_table_checksum_with_warn(
        UACPI_VIRT_ADDR_TO_PTR(table->virt_addr),
        table->length
    );
    if (uacpi_unlikely_error(ret))
        goto out_bad_table;

    if (table->signature.as_u32 == fadt_signature.as_u32) {
        struct acpi_fadt *fadt = UACPI_VIRT_ADDR_TO_PTR(table->virt_addr);
        ret = uacpi_table_append(fadt->x_dsdt ? fadt->x_dsdt : fadt->dsdt);
    }

    if (table->signature.as_u32 == dsdt_signature.as_u32) {
        struct acpi_dsdt *dsdt = UACPI_VIRT_ADDR_TO_PTR(table->virt_addr);
        g_uacpi_rt_ctx.is_rev1 = dsdt->hdr.revision < 2;
    }

    if (out_table == UACPI_NULL)
        do_release_table(table);
    else
        *out_table = table;

    return ret;

out_bad_table:
    table->flags |= UACPI_TABLE_INVALID;
    return ret;
}

uacpi_status uacpi_table_append(uacpi_phys_addr addr)
{
    return uacpi_table_do_append(addr, UACPI_NULL);
}

uacpi_status
uacpi_table_append_acquire(uacpi_phys_addr addr, struct uacpi_table **out_table)
{
    return uacpi_table_do_append(addr, out_table);
}

uacpi_status
uacpi_table_append_mapped(uacpi_virt_addr virt_addr, struct uacpi_table **out_table)
{
    union uacpi_table_signature signature;
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

    table->phys_addr = 0;
    table->virt_addr = virt_addr;
    table->refs = 0;
    table->flags = UACPI_TABLE_NO_UNMAP;
    table->length = hdr->length;

    ret = uacpi_verify_table_checksum_with_warn(hdr, table->length);
    if (uacpi_unlikely_error(ret)) {
        table->flags |= UACPI_TABLE_INVALID;
        return ret;
    }

    if (out_table) {
        *out_table = table;
        return do_acquire_table(table);
    }

    return UACPI_STATUS_OK;
}

struct table_search_spec {
    union uacpi_table_signature signature;
    bool has_signature;

    enum uacpi_table_type type;
    bool has_type;

    uacpi_size base_idx;
};

static uacpi_status do_search_and_acquire(struct table_search_spec *spec,
                                          struct uacpi_table **out_table)
{
    uacpi_size idx;
    uacpi_status ret = UACPI_STATUS_NOT_FOUND;
    struct uacpi_table *found_table = UACPI_NULL;

    for (idx = spec->base_idx;
        idx < table_array_size(&g_uacpi_rt_ctx.tables); ++idx
    ) {
        uacpi_size real_idx = idx + UACPI_BASE_TABLE_COUNT;
        struct uacpi_table *table;

        table = table_array_at(&g_uacpi_rt_ctx.tables, real_idx);

        if (spec->has_signature &&
            spec->signature.as_u32 == table->signature.as_u32) {
            found_table = table;
            break;
        }

        if (spec->has_type && spec->type == table->type) {
            found_table = table;
            break;
        }
    }

    if (found_table == UACPI_NULL)
        return ret;

    ret = do_acquire_table(found_table);
    if (uacpi_likely_success(ret))
        *out_table = found_table;

    return ret;
}

uacpi_status
uacpi_table_acquire_by_type(enum uacpi_table_type type,
                            struct uacpi_table **out_table)
{
    *out_table = get_table_for_type(type);

    if (*out_table == UACPI_NULL) {
        struct table_search_spec spec = {
            .type = type,
            .has_type = true,
            .base_idx = 0
        };

        return do_search_and_acquire(&spec, out_table);
    }

    return do_acquire_table(*out_table);
}

uacpi_status
uacpi_table_acquire_by_signature(union uacpi_table_signature signature,
                                 struct uacpi_table **out_table)
{
    *out_table = get_table_for_signature(signature);

    if (*out_table == UACPI_NULL) {
        struct table_search_spec spec = {
            .signature = signature,
            .has_signature = true,
            .base_idx = 0
        };

        return do_search_and_acquire(&spec, out_table);
    } else {
        return do_acquire_table(*out_table);
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
uacpi_table_acquire_next_with_same_signature(struct uacpi_table **in_out_table)
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
    return do_search_and_acquire(&spec, in_out_table);
}

uacpi_status
uacpi_table_release(struct uacpi_table *table)
{
    return do_release_table(table);
}
