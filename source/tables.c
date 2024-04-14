#include <uacpi/internal/tables.h>
#include <uacpi/internal/utilities.h>
#include <uacpi/internal/stdlib.h>

#define UACPI_STATIC_TABLE_ARRAY_LEN 16

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(
    table_array, uacpi_table, UACPI_STATIC_TABLE_ARRAY_LEN
)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(table_array, struct uacpi_table,)

// FADT + DSDT have a hardcoded index in the array
#define UACPI_BASE_TABLE_COUNT 2

static struct table_array tables;

uacpi_status uacpi_initialize_tables(void)
{
    tables.size_including_inline = UACPI_BASE_TABLE_COUNT;
    return UACPI_STATUS_OK;
}

static uacpi_status initialize_fadt(struct uacpi_table*);

static uacpi_object_name fadt_signature = {
    .text = { ACPI_FADT_SIGNATURE },
};

static uacpi_object_name dsdt_signature = {
    .text = { ACPI_DSDT_SIGNATURE },
};

static uacpi_object_name facs_signature = {
    .text = { ACPI_FACS_SIGNATURE },
};

// TODO: thread safety/locking
static uacpi_status table_do_alloc_slot(struct uacpi_table **out_table)
{
    *out_table = table_array_calloc(&tables);
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
        return table_array_at(&tables, type);
    default:
        break;
    }

    return UACPI_NULL;
}

static struct uacpi_table*
get_table_for_signature(uacpi_object_name signature)
{
    enum uacpi_table_type type = UACPI_TABLE_TYPE_INVALID;

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
    if (*out_table) {
        if ((*out_table)->flags & UACPI_TABLE_VALID)
            return UACPI_STATUS_ALREADY_EXISTS;

        return UACPI_STATUS_OK;
    }

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

    if (*out_len < sizeof(struct acpi_sdt_hdr)) {
        uacpi_error("invalid table %.4s size: %u\n", out_sign->text, *out_len);
        return UACPI_STATUS_INVALID_TABLE_LENGTH;
    }

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

    /*
     * FACS is the only(?) table without a checksum because it has OSPM
     * writable fields. Don't try to validate it here.
     */
    if (table->signature.id != facs_signature.id) {
        ret = uacpi_verify_table_checksum_with_warn(
            UACPI_VIRT_ADDR_TO_PTR(table->virt_addr),
            table->length
        );
        if (uacpi_unlikely_error(ret))
            goto out_bad_table;
    }

    if (table->signature.id == fadt_signature.id) {
        ret = initialize_fadt(table);
        if (uacpi_unlikely_error(ret))
            goto out_bad_table;
    } else if (table->signature.id == dsdt_signature.id) {
        g_uacpi_rt_ctx.is_rev1 = table->hdr->revision < 2;
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
    table->signature = signature;

    ret = uacpi_verify_table_checksum_with_warn(hdr, table->length);
    if (uacpi_unlikely_error(ret))
        return ret;

    if (out_table != UACPI_NULL)
        *out_table = table;

    table->flags |= UACPI_TABLE_VALID;
    return UACPI_STATUS_OK;
}

static uacpi_bool is_valid_table(struct uacpi_table *table)
{
    return table->flags & UACPI_TABLE_VALID;
}

void uacpi_for_each_table(
    uacpi_size base_idx, uacpi_table_iteration_callback cb, void *user
)
{
    uacpi_size idx;
    uacpi_table *table;
    enum uacpi_table_iteration_decision ret;

    for (idx = base_idx; idx < table_array_size(&tables); ++idx) {
        table = table_array_at(&tables, idx);

        if (!is_valid_table(table))
            continue;

        ret = cb(user, table);
        if (ret == UACPI_TABLE_ITERATION_DECISION_BREAK)
            return;
    }
}

struct table_search_ctx {
    struct uacpi_table_identifiers *id;
    uacpi_table *out_table;
};

static enum uacpi_table_iteration_decision do_search_tables(
    void *user, uacpi_table *table
)
{
    struct table_search_ctx *ctx = user;
    struct uacpi_table_identifiers *id = ctx->id;

    if (id->signature.id != table->signature.id)
        return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

    if (id->oemid[0] != '\0' &&
        uacpi_memcmp(id->oemid, table->hdr->oemid, sizeof(id->oemid)) != 0)
        return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

    if (id->oem_table_id[0] != '\0' &&
        uacpi_memcmp(id->oem_table_id, table->hdr->oem_table_id,
                     sizeof(id->oem_table_id)) != 0)
        return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

    ctx->out_table = table;
    return UACPI_TABLE_ITERATION_DECISION_BREAK;
}

static uacpi_status find_table(
    uacpi_size base_idx, struct uacpi_table_identifiers *id,
    uacpi_table **out_table
)
{
    struct table_search_ctx ctx = { .id = id };

    uacpi_for_each_table(base_idx, do_search_tables, &ctx);
    if (ctx.out_table == UACPI_NULL)
        return UACPI_STATUS_NOT_FOUND;

    *out_table = ctx.out_table;
    return UACPI_STATUS_OK;
}

uacpi_status
uacpi_table_find_by_type(enum uacpi_table_type type,
                         struct uacpi_table **out_table)
{
    *out_table = get_table_for_type(type);
    if (*out_table == UACPI_NULL || !is_valid_table(*out_table))
        return UACPI_STATUS_NOT_FOUND;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_table_find_by_signature(
    uacpi_object_name signature, struct uacpi_table **out_table
)
{
    struct table_search_ctx ctx = { 0 };

    ctx.out_table = get_table_for_signature(signature);
    if (ctx.out_table == UACPI_NULL) {
        struct uacpi_table_identifiers id = { .signature = signature, };
        ctx.id = &id;
        uacpi_for_each_table(0, do_search_tables, &ctx);
    }

    if (ctx.out_table == UACPI_NULL || !is_valid_table(ctx.out_table))
        return UACPI_STATUS_NOT_FOUND;

    *out_table = ctx.out_table;
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
    if (tbl >= arr->inline_storage && tbl < end)
        return tbl - arr->inline_storage;

    end = arr->dynamic_storage + (arr->size_including_inline - inline_cap);
    if (tbl >= arr->dynamic_storage && tbl < end)
        return inline_cap + (tbl - arr->dynamic_storage);

    return 0;
}

uacpi_status
uacpi_table_find_next_with_same_signature(struct uacpi_table **in_out_table)
{
    struct uacpi_table_identifiers id = {
        .signature = (*in_out_table)->signature,
    };
    uacpi_u32 base_idx;

    // Tables like FADT, DSDT etc. are always unique, we can fail this right away
    if (get_table_for_signature(id.signature))
        return UACPI_STATUS_NOT_FOUND;

    base_idx = table_array_index_of(&tables, *in_out_table);
    if (base_idx == 0)
        return UACPI_STATUS_INVALID_ARGUMENT;

    return find_table(base_idx + 1, &id, in_out_table);
}

uacpi_status
uacpi_table_find(struct uacpi_table_identifiers *id,
                 struct uacpi_table **out_table)
{
    return find_table(0, id, out_table);
}

uacpi_u16 fadt_version_sizes[] = {
    116, 132, 244, 244, 268, 276
};

static void fadt_ensure_correct_revision(struct acpi_fadt *fadt)
{
    uacpi_size current_rev, rev;

    current_rev = fadt->hdr.revision;

    for (rev = 0; rev < UACPI_ARRAY_SIZE(fadt_version_sizes); ++rev) {
        if (fadt->hdr.length <= fadt_version_sizes[rev])
            break;
    }

    if (rev == UACPI_ARRAY_SIZE(fadt_version_sizes)) {
        uacpi_trace(
            "FADT revision (%zu) is likely greater than the last "
            "supported, reducing to %zu\n", current_rev, rev
        );
        fadt->hdr.revision = rev;
        return;
    }

    rev++;

    if (current_rev != rev && !(rev == 3 && current_rev == 4)) {
        uacpi_warn(
            "FADT length %u doesn't match expected for revision %zu, "
            "assuming version %zu\n", fadt->hdr.length, current_rev,
            rev
        );
        fadt->hdr.revision = rev;
    }
}

static void gas_init_system_io(
    struct acpi_gas *gas, uacpi_u64 address, uacpi_u8 byte_size
)
{
    gas->address = address;
    gas->address_space_id = UACPI_ADDRESS_SPACE_SYSTEM_IO;
    gas->register_bit_width = UACPI_MIN(255, byte_size * 8);
    gas->register_bit_offset = 0;
    gas->access_size = 0;
}


struct register_description {
    uacpi_size offset, xoffset;
    uacpi_size length_offset;
};

#define fadt_offset(field) uacpi_offsetof(struct acpi_fadt, field)

/*
 * We convert all the legacy registers into GAS format and write them into
 * the x_* fields for convenience and faster access at runtime.
 */
static struct register_description fadt_registers[] = {
    {
        .offset = fadt_offset(pm1a_evt_blk),
        .xoffset = fadt_offset(x_pm1a_evt_blk),
        .length_offset = fadt_offset(pm1_evt_len),
    },
    {
        .offset = fadt_offset(pm1b_evt_blk),
        .xoffset = fadt_offset(x_pm1b_evt_blk),
        .length_offset = fadt_offset(pm1_evt_len),
    },
    {
        .offset = fadt_offset(pm1a_cnt_blk),
        .xoffset = fadt_offset(x_pm1a_cnt_blk),
        .length_offset = fadt_offset(pm1_cnt_len),
    },
    {
        .offset = fadt_offset(pm1b_cnt_blk),
        .xoffset = fadt_offset(x_pm1b_cnt_blk),
        .length_offset = fadt_offset(pm1_cnt_len),
    },
    {
        .offset = fadt_offset(pm2_cnt_blk),
        .xoffset = fadt_offset(x_pm2_cnt_blk),
        .length_offset = fadt_offset(pm2_cnt_len),
    },
    {
        .offset = fadt_offset(pm_tmr_blk),
        .xoffset = fadt_offset(x_pm_tmr_blk),
        .length_offset = fadt_offset(pm_tmr_len),
    },
    {
        .offset = fadt_offset(gpe0_blk),
        .xoffset = fadt_offset(x_gpe0_blk),
        .length_offset = fadt_offset(gpe0_blk_len),
    },
    {
        .offset = fadt_offset(gpe1_blk),
        .xoffset = fadt_offset(x_gpe1_blk),
        .length_offset = fadt_offset(gpe1_blk_len),
    },
};

static void *fadt_relative(uacpi_size offset)
{
    return ((uacpi_u8*)&g_uacpi_rt_ctx.fadt) + offset;
}

static void convert_registers_to_gas(void)
{
    uacpi_size i;
    struct register_description *desc;
    struct acpi_gas *gas;
    uacpi_u32 legacy_addr;
    uacpi_u8 length;

    for (i = 0; i < UACPI_ARRAY_SIZE(fadt_registers); ++i) {
        desc = &fadt_registers[i];

        legacy_addr = *(uacpi_u32*)fadt_relative(desc->offset);
        length = *(uacpi_u8*)fadt_relative(desc->length_offset);
        gas = fadt_relative(desc->xoffset);

        if (gas->address)
            continue;

        gas_init_system_io(gas, legacy_addr, length);
    }
}

static void split_one_block(
    struct acpi_gas *src, struct acpi_gas *dst0, struct acpi_gas *dst1
)
{
    uacpi_size byte_length;

    if (src->address == 0)
        return;

    byte_length = src->register_bit_width / 8;
    byte_length /= 2;

    gas_init_system_io(dst0, src->address, byte_length);
    gas_init_system_io(dst1, src->address + byte_length, byte_length);
}

static void split_event_blocks(void)
{
    split_one_block(
        &g_uacpi_rt_ctx.fadt.x_pm1a_evt_blk,
        &g_uacpi_rt_ctx.pm1a_status_blk,
        &g_uacpi_rt_ctx.pm1a_enable_blk
    );
    split_one_block(
        &g_uacpi_rt_ctx.fadt.x_pm1b_evt_blk,
        &g_uacpi_rt_ctx.pm1b_status_blk,
        &g_uacpi_rt_ctx.pm1b_enable_blk
    );
}

static uacpi_status initialize_fadt(struct uacpi_table *tbl)
{
    struct acpi_fadt *fadt = &g_uacpi_rt_ctx.fadt;

    /*
     * Here we (roughly) follow ACPICA initialization sequence to make sure we
     * handle potential BIOS quirks with garbage inside FADT correctly.
     */

    uacpi_memcpy(fadt, tbl->hdr, UACPI_MIN(sizeof(*fadt), tbl->hdr->length));

#if UACPI_REDUCED_HARDWARE == 0
    g_uacpi_rt_ctx.is_hardware_reduced = fadt->flags & ACPI_HW_REDUCED_ACPI;
#endif

    fadt_ensure_correct_revision(fadt);

    /*
     * These are reserved prior to version 3, so zero them out to work around
     * BIOS implementations that might dirty these.
     */
    if (fadt->hdr.revision <= 2) {
        fadt->preferred_pm_profile = 0;
        fadt->pstate_cnt = 0;
        fadt->cst_cnt = 0;
        fadt->iapc_boot_arch = 0;
    }

    if (!fadt->x_dsdt)
        fadt->x_dsdt = fadt->dsdt;

    if (fadt->x_dsdt)
        uacpi_table_append(fadt->x_dsdt, UACPI_NULL);

    if (!uacpi_is_hardware_reduced()) {
        convert_registers_to_gas();
        split_event_blocks();

        /*
         * Unconditionally use 32 bit FACS if it exists, as 64 bit FACS is known
         * to cause issues on some firmware:
         * https://bugzilla.kernel.org/show_bug.cgi?id=74021
         */
        if (fadt->firmware_ctrl)
            fadt->x_firmware_ctrl = fadt->firmware_ctrl;

        if (fadt->x_firmware_ctrl) {
            uacpi_table_append(fadt->x_firmware_ctrl, &tbl);
            g_uacpi_rt_ctx.facs = (struct acpi_facs*)tbl->hdr;
        }
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_table_fadt(struct acpi_fadt **out_fadt)
{
    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level <
                       UACPI_INIT_LEVEL_TABLES_LOADED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    *out_fadt = &g_uacpi_rt_ctx.fadt;
    return UACPI_STATUS_OK;
}
