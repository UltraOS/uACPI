#include <uacpi/internal/tables.h>
#include <uacpi/internal/utilities.h>
#include <uacpi/internal/stdlib.h>
#include <uacpi/internal/interpreter.h>

#ifndef UACPI_STATIC_TABLE_ARRAY_LEN
    #define UACPI_STATIC_TABLE_ARRAY_LEN 16
#endif

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(
    table_array, struct uacpi_installed_table, UACPI_STATIC_TABLE_ARRAY_LEN
)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(
    table_array, struct uacpi_installed_table,
)

static struct table_array tables;
static uacpi_table_installation_handler installation_handler;
static uacpi_handle table_mutex;

uacpi_status uacpi_initialize_tables(void)
{
    table_mutex = uacpi_kernel_create_mutex();
    if (uacpi_unlikely(table_mutex == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    return UACPI_STATUS_OK;
}

void uacpi_deinitialize_tables(void)
{
    uacpi_size i;

    for (i = 0; i < table_array_size(&tables); ++i) {
        struct uacpi_installed_table *tbl = table_array_at(&tables, i);

        switch (tbl->origin) {
        case UACPI_TABLE_ORIGIN_FIRMWARE_VIRTUAL:
            uacpi_free(tbl->ptr, tbl->length);
            break;
        case UACPI_TABLE_ORIGIN_FIRMWARE_PHYSICAL:
        case UACPI_TABLE_ORIGIN_HOST_PHYSICAL:
            uacpi_kernel_unmap(tbl->ptr, tbl->length);
            break;
        default:
            break;
        }
    }

    table_array_clear(&tables);

    if (table_mutex)
        uacpi_kernel_free_mutex(table_mutex);

    installation_handler = UACPI_NULL;
    table_mutex = UACPI_NULL;
}

uacpi_status uacpi_set_table_installation_handler(
    uacpi_table_installation_handler handler
)
{
    uacpi_status ret = UACPI_STATUS_OK;

    /*
     * The mutex might not exist yet because uacpi_initialize_tables might not
     * have been called at this point, allow that possibility since the user
     * might want to install this handler early.
     */
    if (table_mutex != UACPI_NULL)
        UACPI_MUTEX_ACQUIRE(table_mutex);

    if (installation_handler != UACPI_NULL && handler != UACPI_NULL)
        goto out;

    installation_handler = handler;

out:
    if (table_mutex != UACPI_NULL)
        UACPI_MUTEX_RELEASE(table_mutex);
    return ret;
}

static uacpi_status initialize_fadt(struct acpi_sdt_hdr*);

static uacpi_object_name fadt_signature = {
    .text = { ACPI_FADT_SIGNATURE },
};

static uacpi_object_name dsdt_signature = {
    .text = { ACPI_DSDT_SIGNATURE },
};

static uacpi_object_name facs_signature = {
    .text = { ACPI_FACS_SIGNATURE },
};

static uacpi_u8 table_checksum(void *table, uacpi_size size)
{
    uacpi_u8 *bytes = table;
    uacpi_u8 csum = 0;
    uacpi_size i;

    for (i = 0; i < size; ++i)
        csum += bytes[i];

    return csum;
}

uacpi_status uacpi_verify_table_checksum(void *table, uacpi_size size)
{
    uacpi_status ret = UACPI_STATUS_OK;
    uacpi_u8 csum;

    csum = table_checksum(table, size);

    if (uacpi_unlikely(csum != 0)) {
        enum uacpi_log_level lvl = UACPI_LOG_WARN;
        struct acpi_sdt_hdr *hdr = table;

        if (uacpi_check_flag(UACPI_FLAG_BAD_CSUM_FATAL)) {
            ret = UACPI_STATUS_BAD_CHECKSUM;
            lvl = UACPI_LOG_ERROR;
        }

        uacpi_log_lvl(
            lvl, "invalid table "UACPI_PRI_TBL_HDR" checksum %d!\n",
            UACPI_FMT_TBL_HDR(hdr), csum
        );
    }

    return ret;
}

uacpi_status uacpi_check_table_signature(void *table, const uacpi_char *expect)
{
    uacpi_status ret = UACPI_STATUS_OK;

    if (uacpi_memcmp(table, expect, 4) != 0) {
        enum uacpi_log_level lvl = UACPI_LOG_WARN;
        struct acpi_sdt_hdr *hdr = table;

        if (uacpi_check_flag(UACPI_FLAG_BAD_TBL_SIGNATURE_FATAL)) {
            ret = UACPI_STATUS_INVALID_SIGNATURE;
            lvl = UACPI_LOG_ERROR;
        }

        uacpi_log_lvl(
            lvl,
            "invalid table "UACPI_PRI_TBL_HDR" signature (expected '%.4s')\n",
            UACPI_FMT_TBL_HDR(hdr), expect
        );
    }

    return ret;
}

static uacpi_status table_alloc(
    struct uacpi_installed_table **out_tbl, uacpi_size *out_idx
)
{
    struct uacpi_installed_table *tbl;

    tbl = table_array_alloc(&tables);
    if (uacpi_unlikely(tbl == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    *out_tbl = tbl;
    *out_idx = table_array_size(&tables) - 1;
    return UACPI_STATUS_OK;
}

static uacpi_status
get_external_table_signature_and_length(uacpi_phys_addr phys_addr,
                                        uacpi_object_name *out_sign,
                                        uacpi_u32 *out_len)
{
    struct acpi_sdt_hdr *hdr;

    hdr = uacpi_kernel_map(phys_addr, sizeof(struct acpi_sdt_hdr));
    if (uacpi_unlikely(!hdr))
        return UACPI_STATUS_MAPPING_FAILED;

    uacpi_memcpy(out_sign, hdr->signature, sizeof(hdr->signature));
    *out_len = hdr->length;

    uacpi_kernel_unmap(hdr, sizeof(struct acpi_sdt_hdr));
    return UACPI_STATUS_OK;
}

static uacpi_status verify_and_install_table(
    uacpi_object_name signature, uacpi_u32 length,
    uacpi_phys_addr phys_addr, void *virt_addr, enum uacpi_table_origin origin,
    uacpi_table *out_table
)
{
    uacpi_status ret;
    struct uacpi_installed_table *table;
    uacpi_size idx;

    /*
     * FACS is the only(?) table without a checksum because it has OSPM
     * writable fields. Don't try to validate it here.
     */
    if (signature.id != facs_signature.id) {
        ret = uacpi_verify_table_checksum(virt_addr, length);
        if (uacpi_unlikely_error(ret))
            return ret;
    }

    if (signature.id == dsdt_signature.id) {
        struct acpi_sdt_hdr *hdr = virt_addr;
        g_uacpi_rt_ctx.is_rev1 = hdr->revision < 2;
    }

    if (signature.id == fadt_signature.id) {
        ret = initialize_fadt(virt_addr);
        if (uacpi_unlikely_error(ret))
            return ret;
    }

    ret = table_alloc(&table, &idx);
    if (uacpi_unlikely_error(ret))
        return ret;

    table->signature = signature;
    table->phys_addr = phys_addr;
    table->ptr = virt_addr;
    table->length = length;
    table->flags = 0;
    table->origin = origin;

    if (out_table != UACPI_NULL) {
        out_table->ptr = virt_addr;
        out_table->index = idx;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status table_install_physical_with_origin_unlocked(
    uacpi_phys_addr phys, enum uacpi_table_origin origin,
    const uacpi_char *expected_signature, uacpi_table *out_table
);
static uacpi_status table_install_with_origin_unlocked(
    void *virt, enum uacpi_table_origin origin, uacpi_table *out_table
);

static uacpi_status handle_table_override(
    uacpi_table_installation_disposition disposition, uacpi_u64 address,
    uacpi_table *out_table
)
{
    uacpi_status ret;

    switch (disposition) {
    case UACPI_TABLE_INSTALLATION_DISPOSITON_VIRTUAL_OVERRIDE:
        ret = table_install_with_origin_unlocked(
            UACPI_VIRT_ADDR_TO_PTR((uacpi_virt_addr)address),
            UACPI_TABLE_ORIGIN_HOST_VIRTUAL,
            out_table
        );
        return ret;
    case UACPI_TABLE_INSTALLATION_DISPOSITON_PHYSICAL_OVERRIDE:
        return table_install_physical_with_origin_unlocked(
            (uacpi_phys_addr)address,
            UACPI_TABLE_ORIGIN_HOST_PHYSICAL,
            UACPI_NULL,
            out_table
        );
    default:
        uacpi_error("invalid table installation disposition %d\n", disposition);
        return UACPI_STATUS_INTERNAL_ERROR;
    }
}

static uacpi_status table_install_physical_with_origin_unlocked(
    uacpi_phys_addr phys, enum uacpi_table_origin origin,
    const uacpi_char *expected_signature, uacpi_table *out_table
)
{
    uacpi_object_name signature;
    uacpi_u32 length;
    void *virt;
    uacpi_status ret;

    ret = get_external_table_signature_and_length(phys, &signature, &length);
    if (uacpi_unlikely_error(ret))
        return ret;

    if (uacpi_unlikely(length < sizeof(struct acpi_sdt_hdr))) {
        uacpi_error("invalid table '%.4s' (0x016%"UACPI_PRIX64") size: %u\n",
                    signature.text, UACPI_FMT64(phys), length);
        return UACPI_STATUS_INVALID_TABLE_LENGTH;
    }

    virt = uacpi_kernel_map(phys, length);
    if (uacpi_unlikely(!virt))
        return UACPI_STATUS_MAPPING_FAILED;

    if (expected_signature != UACPI_NULL) {
        ret = uacpi_check_table_signature(virt, expected_signature);
        if (uacpi_unlikely_error(ret))
            goto out;
    }

    if (origin == UACPI_TABLE_ORIGIN_FIRMWARE_PHYSICAL &&
        installation_handler != UACPI_NULL) {
        uacpi_u64 override;
        uacpi_table_installation_disposition disposition;

        disposition = installation_handler(virt, &override);

        switch (disposition) {
        case UACPI_TABLE_INSTALLATION_DISPOSITON_ALLOW:
            break;
        case UACPI_TABLE_INSTALLATION_DISPOSITON_DENY:
            uacpi_info(
                "table '%.4s' (0x016%"UACPI_PRIX64") installation denied "
                "by host\n", signature.text, UACPI_FMT64(phys)
            );
            ret = UACPI_STATUS_DENIED;
            goto out;

        default:
            uacpi_info(
                "table '%.4s' (0x016%"UACPI_PRIX64") installation "
                "overriden by host\n", signature.text, UACPI_FMT64(phys)
            );

            ret = handle_table_override(disposition, override, out_table);
            if (uacpi_likely_success(ret))
                ret = UACPI_STATUS_OVERRIDEN;

            goto out;
        }
    }

    ret = verify_and_install_table(signature, length, phys, virt, origin,
                                   out_table);
out:
    if (uacpi_unlikely_error(ret)) {
        uacpi_kernel_unmap(virt, length);
        return ret;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_table_install_physical_with_origin(
    uacpi_phys_addr phys, enum uacpi_table_origin origin, uacpi_table *out_table
)
{
    uacpi_status ret;

    UACPI_MUTEX_ACQUIRE(table_mutex);
    ret = table_install_physical_with_origin_unlocked(
        phys, origin, UACPI_NULL, out_table
    );
    UACPI_MUTEX_RELEASE(table_mutex);

    return ret;
}

static uacpi_status table_install_with_origin_unlocked(
    void *virt, enum uacpi_table_origin origin, uacpi_table *out_table
)
{
    uacpi_object_name signature;
    struct acpi_sdt_hdr *hdr = virt;
    uacpi_u32 length;

    uacpi_memcpy(&signature, hdr->signature, sizeof(hdr->signature));
    length = hdr->length;

    if (uacpi_unlikely(length < sizeof(struct acpi_sdt_hdr))) {
        uacpi_error("invalid table '%.4s' (%p) size: %u\n",
                    signature.text, virt, length);
        return UACPI_STATUS_INVALID_TABLE_LENGTH;
    }

    if (origin == UACPI_TABLE_ORIGIN_FIRMWARE_VIRTUAL &&
        installation_handler != UACPI_NULL) {
        uacpi_u64 override;
        uacpi_table_installation_disposition disposition;

        disposition = installation_handler(virt, &override);

        switch (disposition) {
        case UACPI_TABLE_INSTALLATION_DISPOSITON_ALLOW:
            break;
        case UACPI_TABLE_INSTALLATION_DISPOSITON_DENY:
            uacpi_info(
                "table "UACPI_PRI_TBL_HDR" installation denied by host\n",
                UACPI_FMT_TBL_HDR(hdr)
            );
            return UACPI_STATUS_DENIED;

        default: {
            uacpi_status ret;
            uacpi_info(
                "table "UACPI_PRI_TBL_HDR" installation overriden by host\n",
                UACPI_FMT_TBL_HDR(hdr)
            );

            ret = handle_table_override(disposition, override, out_table);
            if (uacpi_likely_success(ret))
                ret = UACPI_STATUS_OVERRIDEN;

            return ret;
        }
        }
    }

    return verify_and_install_table(
        signature, length, 0, virt, origin, out_table
    );
}

uacpi_status uacpi_table_install_with_origin(
    void *virt, enum uacpi_table_origin origin, uacpi_table *out_table
)
{
    uacpi_status ret;

    UACPI_MUTEX_ACQUIRE(table_mutex);
    ret = table_install_with_origin_unlocked(virt, origin, out_table);
    UACPI_MUTEX_RELEASE(table_mutex);

    return ret;
}

uacpi_status uacpi_table_install(void *virt, uacpi_table *out_table)
{
    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);

    return uacpi_table_install_with_origin(
        virt, UACPI_TABLE_ORIGIN_HOST_VIRTUAL, out_table
    );
}

uacpi_status uacpi_table_install_physical(
    uacpi_phys_addr addr, uacpi_table *out_table
)
{
    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);

    return uacpi_table_install_physical_with_origin(
        addr, UACPI_TABLE_ORIGIN_HOST_PHYSICAL, out_table
    );
}

uacpi_status uacpi_for_each_table(
    uacpi_size base_idx, uacpi_table_iteration_callback cb, void *user
)
{
    uacpi_size idx;
    struct uacpi_installed_table *tbl;
    enum uacpi_table_iteration_decision ret;

    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);

    UACPI_MUTEX_ACQUIRE(table_mutex);

    for (idx = base_idx; idx < table_array_size(&tables); ++idx) {
        tbl = table_array_at(&tables, idx);

        ret = cb(user, tbl, idx);
        if (ret == UACPI_TABLE_ITERATION_DECISION_BREAK)
            break;
    }

    UACPI_MUTEX_RELEASE(table_mutex);
    return UACPI_STATUS_OK;
}

enum search_type {
    SEARCH_TYPE_BY_ID,
    SEARCH_TYPE_MATCH,
};

struct table_search_ctx {
    union {
        const uacpi_table_identifiers *id;
        uacpi_table_match_callback match_cb;
    };

    uacpi_table *out_table;
    uacpi_u8 search_type;
    uacpi_bool found;
};

static enum uacpi_table_iteration_decision do_search_tables(
    void *user, struct uacpi_installed_table *tbl, uacpi_size idx
)
{
    struct table_search_ctx *ctx = user;
    uacpi_table *out_table;

    switch (ctx->search_type) {
    case SEARCH_TYPE_BY_ID: {
        const uacpi_table_identifiers *id = ctx->id;

        if (id->signature.id != tbl->signature.id)
            return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

        if (id->oemid[0] != '\0' &&
            uacpi_memcmp(id->oemid, tbl->hdr->oemid, sizeof(id->oemid)) != 0)
            return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

        if (id->oem_table_id[0] != '\0' &&
            uacpi_memcmp(id->oem_table_id, tbl->hdr->oem_table_id,
                         sizeof(id->oem_table_id)) != 0)
            return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

        break;
    }

    case SEARCH_TYPE_MATCH:
        if (!ctx->match_cb(tbl))
            return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

    }

    out_table = ctx->out_table;
    out_table->ptr = tbl->ptr;
    out_table->index = idx;
    ctx->found = UACPI_TRUE;

    return UACPI_TABLE_ITERATION_DECISION_BREAK;
}

uacpi_status uacpi_table_match(
    uacpi_size base_idx, uacpi_table_match_callback cb, uacpi_table *out_table
)
{
    struct table_search_ctx  ctx = {
        .match_cb = cb,
        .search_type = SEARCH_TYPE_MATCH,
        .out_table = out_table,
    };

    uacpi_for_each_table(base_idx, do_search_tables, &ctx);
    if (!ctx.found)
        return UACPI_STATUS_NOT_FOUND;

    return UACPI_STATUS_OK;
}


static uacpi_status find_table(
    uacpi_size base_idx, const uacpi_table_identifiers *id,
    uacpi_table *out_table
)
{
    struct table_search_ctx ctx = {
        .id = id,
        .out_table = out_table,
        .search_type = SEARCH_TYPE_BY_ID,
    };

    uacpi_for_each_table(base_idx, do_search_tables, &ctx);
    if (!ctx.found)
        return UACPI_STATUS_NOT_FOUND;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_table_find_by_signature(
    const uacpi_char *signature_string, struct uacpi_table *out_table
)
{
    struct uacpi_table_identifiers id = {
        .signature = {
            .text = {
                signature_string[0],
                signature_string[1],
                signature_string[2],
                signature_string[3]
            }
        }
    };

    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);
    return find_table(0, &id, out_table);
}

uacpi_status
uacpi_table_find_next_with_same_signature(uacpi_table *in_out_table)
{
    struct uacpi_table_identifiers id = { 0 };

    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);

    uacpi_memcpy(&id.signature, in_out_table->hdr->signature,
                 sizeof(id.signature));

    return find_table(in_out_table->index + 1, &id, in_out_table);
}

uacpi_status uacpi_table_find(
    const uacpi_table_identifiers *id, uacpi_table *out_table
)
{
    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);
    return find_table(0, id, out_table);
}

static uacpi_status table_ctl(
    uacpi_size idx, uacpi_u8 expect_set, uacpi_u8 expect_clear,
    uacpi_u8 set, uacpi_u8 clear, void **out_tbl
)
{
    uacpi_status ret = UACPI_STATUS_OK;
    struct uacpi_installed_table *tbl;

    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);

    UACPI_MUTEX_ACQUIRE(table_mutex);
    if (uacpi_unlikely(table_array_size(&tables) <= idx)) {
        uacpi_error(
            "requested invalid table index %zu (%zu tables installed)\n",
            idx, table_array_size(&tables)
        );
        ret = UACPI_STATUS_INVALID_ARGUMENT;
        goto out;
    }

    tbl = table_array_at(&tables, idx);
    if (uacpi_unlikely((tbl->flags & expect_set) != expect_set)) {
        uacpi_error(
            "unexpected table '%.4s' flags %02X, expected %02X to be set\n",
            tbl->signature.text, tbl->flags, expect_set
        );
        ret = UACPI_STATUS_INVALID_ARGUMENT;
        goto out;
    }

    if (uacpi_unlikely((tbl->flags & expect_clear) != 0)) {
        uacpi_error(
            "unexpected table '%.4s' flags %02X, expected %02X to be clear\n",
            tbl->signature.text, tbl->flags, expect_clear
        );
        ret = UACPI_STATUS_ALREADY_EXISTS;
        goto out;
    }

    if (out_tbl != UACPI_NULL)
        *out_tbl = tbl->ptr;

    tbl->flags |= set;
    tbl->flags &= ~clear;

out:
    UACPI_MUTEX_RELEASE(table_mutex);
    return ret;
}

uacpi_status uacpi_table_load_with_cause(
    uacpi_size idx, enum uacpi_table_load_cause cause
)
{
    uacpi_status ret;
    void *tbl;
    uacpi_u8 set = UACPI_TABLE_LOADED;
    uacpi_u8 expect_clear = set;

    ret = table_ctl(idx, 0, expect_clear, set, 0, &tbl);
    if (uacpi_unlikely_error(ret))
        return ret;

    return uacpi_execute_table(tbl, cause);
}

uacpi_status uacpi_table_load(uacpi_size idx)
{
    return uacpi_table_load_with_cause(idx, UACPI_TABLE_LOAD_CAUSE_HOST);
}

void uacpi_table_mark_as_loaded(uacpi_size idx)
{
    table_ctl(idx, 0, 0, UACPI_TABLE_LOADED, 0, UACPI_NULL);
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

static uacpi_status initialize_fadt(struct acpi_sdt_hdr *hdr)
{
    uacpi_status ret;
    uacpi_table tbl;
    struct acpi_fadt *fadt = &g_uacpi_rt_ctx.fadt;

    /*
     * Here we (roughly) follow ACPICA initialization sequence to make sure we
     * handle potential BIOS quirks with garbage inside FADT correctly.
     */

    uacpi_memcpy(fadt, hdr, UACPI_MIN(sizeof(*fadt), hdr->length));

#ifndef UACPI_REDUCED_HARDWARE
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

    if (fadt->x_dsdt) {
        ret = table_install_physical_with_origin_unlocked(
            fadt->x_dsdt, UACPI_TABLE_ORIGIN_FIRMWARE_PHYSICAL,
            ACPI_DSDT_SIGNATURE, &tbl
        );
        if (uacpi_unlikely_error(ret))
            return ret;
    }

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
            ret = table_install_physical_with_origin_unlocked(
                fadt->x_firmware_ctrl, UACPI_TABLE_ORIGIN_FIRMWARE_PHYSICAL,
                ACPI_FACS_SIGNATURE, &tbl
            );
            if (uacpi_unlikely_error(ret))
                return ret;

            g_uacpi_rt_ctx.facs = tbl.ptr;
        }
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_table_fadt(struct acpi_fadt **out_fadt)
{
    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_TABLES_LOADED);

    *out_fadt = &g_uacpi_rt_ctx.fadt;
    return UACPI_STATUS_OK;
}
