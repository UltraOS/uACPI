#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>

#include <uacpi/internal/log.h>
#include <uacpi/internal/context.h>
#include <uacpi/internal/utilities.h>
#include <uacpi/internal/tables.h>
#include <uacpi/internal/interpreter.h>
#include <uacpi/internal/namespace.h>

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
    case UACPI_STATUS_ALREADY_EXISTS:
        return "already exists";
    case UACPI_STATUS_INTERNAL_ERROR:
        return "internal error";
    case UACPI_STATUS_TYPE_MISMATCH:
        return "object type mismatch";
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
        ret = uacpi_table_append(entry_addr, UACPI_NULL);
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
    uacpi_status ret;
    struct acpi_rsdp *rsdp;
    uacpi_phys_addr rxsdt;
    uacpi_size rxsdt_entry_size;

    g_uacpi_rt_ctx.is_rev1 = UACPI_TRUE;
    g_uacpi_rt_ctx.tables.size_including_inline = UACPI_BASE_TABLE_COUNT;

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

    ret = initialize_from_rxsdt(rxsdt, rxsdt_entry_size);
    if (uacpi_unlikely_error(ret))
        return ret;

    ret = uacpi_namespace_initialize_predefined();
    if (uacpi_unlikely_error(ret))
        return ret;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_namespace_load(void)
{
    struct uacpi_table *tbl;
    uacpi_status ret;

    ret = uacpi_table_find_by_type(UACPI_TABLE_TYPE_DSDT, &tbl);
    if (uacpi_unlikely_error(ret))
        return ret;

    ret = uacpi_load_table(tbl);
    if (uacpi_unlikely_error(ret))
        return ret;

    ret = uacpi_table_find_by_type(UACPI_TABLE_TYPE_SSDT, &tbl);
    if (ret != UACPI_STATUS_OK)
        goto out;

    do {
        ret = uacpi_load_table(tbl);
        if (uacpi_unlikely_error(ret))
            return ret;

        ret = uacpi_table_find_next_with_same_signature(&tbl);
    } while (ret == UACPI_STATUS_OK);

out:
    if (ret == UACPI_STATUS_NOT_FOUND)
        ret = UACPI_STATUS_OK;

    return ret;
}

struct ns_init_context {
    uacpi_size ini_executed;
    uacpi_size ini_errors;
    uacpi_size sta_executed;
    uacpi_size sta_errors;
    uacpi_size devices;
    uacpi_size thermal_zones;
    uacpi_size processors;
};

static void do_account_sta_ini(
    const uacpi_char *method, uacpi_size *counter, uacpi_size *err_counter,
    uacpi_namespace_node *node, uacpi_status ret
)
{
    const uacpi_char *absolute_path;
    uacpi_char oom_absolute_path[10] = "<...>";

    if (ret == UACPI_STATUS_NOT_FOUND)
        return;

    (*counter)++;

    if (ret == UACPI_STATUS_OK)
        return;

    (*err_counter)++;

    absolute_path = uacpi_namespace_node_generate_absolute_path(node);
    if (absolute_path == UACPI_NULL) {
        absolute_path = oom_absolute_path;
        uacpi_memcpy(oom_absolute_path + 5, node->name.text, 4);
    }

    uacpi_kernel_log(
        UACPI_LOG_WARN,
        "Aborted execution of '%s.%s' due to an error: %s\n",
        absolute_path, method, uacpi_status_to_string(ret)
    );

    if (uacpi_likely(absolute_path != oom_absolute_path))
        uacpi_kernel_free((void*)absolute_path);
}

static void ini_eval(struct ns_init_context *ctx, uacpi_namespace_node *node)
{
    uacpi_status ret;

    ret = uacpi_eval(node, "_INI", NULL, NULL);
    do_account_sta_ini("_INI", &ctx->ini_executed, &ctx->ini_errors, node, ret);
}

static uacpi_status sta_eval(
    struct ns_init_context *ctx, uacpi_namespace_node *node,
    uacpi_u32 *value
)
{
    uacpi_status ret;
    uacpi_object *obj;

    ret = uacpi_eval_typed(node, "_STA", NULL, UACPI_OBJECT_INTEGER_BIT, &obj);
    do_account_sta_ini("_STA", &ctx->sta_executed, &ctx->sta_errors, node, ret);

    /*
     * ACPI 6.5 specification:
     * If a device object (including the processor object) does not have
     * an _STA object, then OSPM assumes that all of the above bits are
     * set (i.e., the device is present, enabled, shown in the UI,
     * and functioning).
     */
    if (ret == UACPI_STATUS_NOT_FOUND) {
        *value = 0xFFFFFFFF;
        return UACPI_STATUS_OK;
    }

    if (ret == UACPI_STATUS_OK) {
        *value = obj->integer;
        uacpi_object_unref(obj);
        return ret;
    }

    return ret;
}

static enum uacpi_ns_iteration_decision do_sta_ini(
    void *opaque, uacpi_namespace_node *node
)
{
    struct ns_init_context *ctx = opaque;
    uacpi_status ret;
    uacpi_u32 sta_ret;
    uacpi_bool is_sb;
    uacpi_object *obj;

    // We don't care about aliases
    if (node->flags & UACPI_NAMESPACE_NODE_FLAG_ALIAS)
        return UACPI_NS_ITERATION_DECISION_NEXT_PEER;

    is_sb = node == uacpi_namespace_get_predefined(
        UACPI_PREDEFINED_NAMESPACE_SB
    );

    obj = uacpi_namespace_node_get_object(node);
    if (node != uacpi_namespace_root() && !is_sb) {
        switch (obj->type) {
        case UACPI_OBJECT_DEVICE:
            ctx->devices++;
            break;
        case UACPI_OBJECT_THERMAL_ZONE:
            ctx->thermal_zones++;
            break;
        case UACPI_OBJECT_PROCESSOR:
            ctx->processors++;
            break;
        default:
            return UACPI_NS_ITERATION_DECISION_CONTINUE;
        }
    }

    ret = sta_eval(ctx, node, &sta_ret);
    if (uacpi_unlikely_error(ret))
        return UACPI_NS_ITERATION_DECISION_CONTINUE;

    if (!(sta_ret & ACPI_STA_RESULT_DEVICE_PRESENT)) {
        if (!(sta_ret & ACPI_STA_RESULT_DEVICE_FUNCTIONING))
            return UACPI_NS_ITERATION_DECISION_NEXT_PEER;

        /*
         * ACPI 6.5 specification:
         * _STA may return bit 0 clear (not present) with bit [3] set (device
         * is functional). This case is used to indicate a valid device for
         * which no device driver should be loaded (for example, a bridge
         * device.) Children of this device may be present and valid. OSPM
         * should continue enumeration below a device whose _STA returns this
         * bit combination.
         */
        return UACPI_NS_ITERATION_DECISION_CONTINUE;
    }

    if (node != uacpi_namespace_root() && !is_sb)
        ini_eval(ctx, node);

    return UACPI_NS_ITERATION_DECISION_CONTINUE;
}

uacpi_status uacpi_namespace_initialize(void)
{
    struct ns_init_context ctx = { 0 };

    /*
     * Initialization order here is identical to ACPICA because ACPI
     * specification doesn't really have any detailed steps that explain
     * how to do it.
     */

    // Step 1 - Execute \_INI
    ini_eval(&ctx, uacpi_namespace_root());

    // Step 2 - Execute \_SB._INI
    ini_eval(
        &ctx, uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB)
    );

    /*
     * Step 3 - This is where we would run _REG methods,
     *          but we don't support that machinery yet.
     * TODO: implement once we have proper support for operation region
     *       override and other similar stuff.
     */

    // Step 4 - Run all other _STA and _INI methods
    uacpi_namespace_for_each_node_depth_first(
        uacpi_namespace_root(), do_sta_ini, &ctx
    );

    uacpi_kernel_log(
        UACPI_LOG_INFO, "Namespace initialization done: "
        "%zu devices, %zu thermal zones, %zu processors\n",
        ctx.devices, ctx.thermal_zones, ctx.processors
    );

    uacpi_kernel_log(
        UACPI_LOG_TRACE,
        "_STA calls: %zu (%zu errors), _INI calls: %zu (%zu errors)\n",
        ctx.sta_executed, ctx.sta_errors, ctx.ini_executed,
        ctx.ini_errors
    );

    return UACPI_STATUS_OK;
}

uacpi_status
uacpi_eval(uacpi_namespace_node *parent, const uacpi_char *path,
           uacpi_args *args, uacpi_object **ret)
{
    struct uacpi_namespace_node *node;
    uacpi_object *obj;

    if (parent == UACPI_NULL && path == UACPI_NULL)
        return UACPI_STATUS_INVALID_ARGUMENT;

    if (path != UACPI_NULL) {
        node = uacpi_namespace_node_find(parent, path);
        if (node == UACPI_NULL)
            return UACPI_STATUS_NOT_FOUND;
    } else {
        node = parent;
    }

    obj = uacpi_namespace_node_get_object(node);
    if (obj->type != UACPI_OBJECT_METHOD) {
        *ret = obj;
        uacpi_object_ref(obj);
        return UACPI_STATUS_OK;
    }

    return uacpi_execute_control_method(node, obj->method, args, ret);
}

uacpi_status uacpi_eval_typed(
    uacpi_namespace_node *parent, const uacpi_char *path,
    uacpi_args *args, uacpi_u32 ret_mask, uacpi_object **ret
)
{
    uacpi_status st;
    uacpi_u32 type_mask = 0;

    st = uacpi_eval(parent, path, args, ret);
    if (uacpi_unlikely_error(st))
        return st;

    if ((*ret) != UACPI_NULL)
        type_mask = 1 << (*ret)->type;

    if (ret_mask && (ret_mask & type_mask) == 0)
        return UACPI_STATUS_TYPE_MISMATCH;

    return st;
}
