#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>

#include <uacpi/internal/log.h>
#include <uacpi/internal/context.h>
#include <uacpi/internal/utilities.h>
#include <uacpi/internal/tables.h>
#include <uacpi/internal/interpreter.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/internal/opregion.h>
#include <uacpi/internal/registers.h>
#include <uacpi/internal/event.h>

struct uacpi_runtime_context g_uacpi_rt_ctx = { 0 };

const uacpi_char *uacpi_status_to_string(uacpi_status st)
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
    case UACPI_STATUS_UNIMPLEMENTED:
        return "unimplemented";
    case UACPI_STATUS_ALREADY_EXISTS:
        return "already exists";
    case UACPI_STATUS_INTERNAL_ERROR:
        return "internal error";
    case UACPI_STATUS_TYPE_MISMATCH:
        return "object type mismatch";
    case UACPI_STATUS_INIT_LEVEL_MISMATCH:
        return "init level too low/high for this action";
    case UACPI_STATUS_NAMESPACE_NODE_DANGLING:
        return "attempting to use a dangling namespace node";
    case UACPI_STATUS_NO_HANDLER:
        return "no handler found";
    case UACPI_STATUS_NO_RESOURCE_END_TAG:
        return "resource template without an end tag";
    case UACPI_STATUS_COMPILED_OUT:
        return "this functionality has been compiled out of this build";
    case UACPI_STATUS_HARDWARE_TIMEOUT:
        return "timed out waiting for hardware response";

    case UACPI_STATUS_AML_UNDEFINED_REFERENCE:
        return "AML referenced an undefined object";
    case UACPI_STATUS_AML_INVALID_NAMESTRING:
        return "invalid AML name string";
    case UACPI_STATUS_AML_OBJECT_ALREADY_EXISTS:
        return "object already exists";
    case UACPI_STATUS_AML_INVALID_OPCODE:
        return "invalid AML opcode";
    case UACPI_STATUS_AML_INCOMPATIBLE_OBJECT_TYPE:
        return "incompatible AML object type";
    case UACPI_STATUS_AML_BAD_ENCODING:
        return "bad AML instruction encoding";
    case UACPI_STATUS_AML_OUT_OF_BOUNDS_INDEX:
        return "out of bounds AML index";
    case UACPI_STATUS_AML_SYNC_LEVEL_TOO_HIGH:
        return "AML attempted to acquire a mutex with a lower sync level";
    case UACPI_STATUS_AML_INVALID_RESOURCE:
        return "invalid resource template encoding or type";
    default:
        return "<invalid status>";
    }
}

#if UACPI_REDUCED_HARDWARE == 0
enum hw_mode {
    HW_MODE_ACPI = 0,
    HW_MODE_LEGACY = 1,
};

static enum hw_mode read_mode(void)
{
    uacpi_status ret;
    uacpi_u64 raw_value;
    struct acpi_fadt *fadt = &g_uacpi_rt_ctx.fadt;

    if (!fadt->smi_cmd)
        return HW_MODE_ACPI;

    ret = uacpi_read_register_field(UACPI_REGISTER_FIELD_SCI_EN, &raw_value);
    if (uacpi_unlikely_error(ret))
        return HW_MODE_LEGACY;

    return raw_value ? HW_MODE_ACPI : HW_MODE_LEGACY;
}

static uacpi_status set_mode(enum hw_mode mode)
{
    uacpi_status ret;
    uacpi_u64 raw_value, stalled_time = 0;
    struct acpi_fadt *fadt = &g_uacpi_rt_ctx.fadt;

    if (uacpi_unlikely(!fadt->smi_cmd)) {
        uacpi_error("SMI_CMD is not implemented by the firmware\n");
        return UACPI_STATUS_NOT_FOUND;
    }

    if (uacpi_unlikely(!fadt->acpi_enable && !fadt->acpi_disable)) {
        uacpi_error("mode transition is not implemented by the hardware\n");
        return UACPI_STATUS_NOT_FOUND;
    }

    switch (mode) {
    case HW_MODE_ACPI:
        raw_value = fadt->acpi_enable;
        break;
    case HW_MODE_LEGACY:
        raw_value = fadt->acpi_disable;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    ret = uacpi_write_register(UACPI_REGISTER_SMI_CMD, raw_value);
    if (uacpi_unlikely_error(ret))
        return ret;

    // Allow up to 5 seconds for the hardware to enter the desired mode
    while (stalled_time < (5 * 1000 * 1000)) {
        if (read_mode() == mode)
            return UACPI_STATUS_OK;

        uacpi_kernel_stall(100);
        stalled_time += 100;
    }

    uacpi_error("hardware time out while changing modes\n");
    return UACPI_STATUS_HARDWARE_TIMEOUT;
}

static uacpi_status enter_mode(enum hw_mode mode)
{
    uacpi_status ret;
    const uacpi_char *mode_str;

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level <
                       UACPI_INIT_LEVEL_TABLES_LOADED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    if (uacpi_is_hardware_reduced())
        return UACPI_STATUS_OK;

    mode_str = mode == HW_MODE_LEGACY ? "legacy" : "acpi";

    if (read_mode() == mode) {
        uacpi_trace("%s mode already enabled\n", mode_str);
        return UACPI_STATUS_OK;
    }

    ret = set_mode(mode);
    if (uacpi_unlikely_error(ret)) {
        uacpi_error(
            "unable to enter %s mode: %s\n",
            mode_str, uacpi_status_to_string(ret)
        );
        return ret;
    }

    uacpi_trace("entered %s mode\n", mode_str);
    return ret;
}

uacpi_status uacpi_enter_acpi_mode(void)
{
    return enter_mode(HW_MODE_ACPI);
}

uacpi_status uacpi_leave_acpi_mode(void)
{
    return enter_mode(HW_MODE_LEGACY);
}
#endif

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

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level !=
                       UACPI_INIT_LEVEL_EARLY))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    g_uacpi_rt_ctx.init_level = UACPI_INIT_LEVEL_TABLES_LOADED;
    g_uacpi_rt_ctx.is_rev1 = UACPI_TRUE;
    g_uacpi_rt_ctx.last_sleep_typ_a = UACPI_SLEEP_TYP_INVALID;
    g_uacpi_rt_ctx.last_sleep_typ_b = UACPI_SLEEP_TYP_INVALID;
    g_uacpi_rt_ctx.s0_sleep_typ_a = UACPI_SLEEP_TYP_INVALID;
    g_uacpi_rt_ctx.s0_sleep_typ_b = UACPI_SLEEP_TYP_INVALID;

    uacpi_memcpy(&g_uacpi_rt_ctx.params, &params->rt_params,
                 sizeof(params->rt_params));

    ret = uacpi_initialize_tables();
    if (uacpi_unlikely_error(ret))
        return ret;

    rsdp = uacpi_kernel_map(params->rsdp, sizeof(struct acpi_rsdp));
    if (rsdp == UACPI_NULL)
        return UACPI_STATUS_MAPPING_FAILED;

    if (rsdp->revision > 1 && rsdp->xsdt_addr &&
        !uacpi_rt_params_check(UACPI_PARAM_BAD_XSDT))
    {
        rxsdt = uacpi_truncate_phys_addr_with_warn(rsdp->xsdt_addr);
        rxsdt_entry_size = 8;
    } else {
        rxsdt = (uacpi_phys_addr)rsdp->rsdt_addr;
        rxsdt_entry_size = 4;
    }

    uacpi_kernel_unmap(rsdp, sizeof(struct acpi_rsdp));

    if (!rxsdt) {
        uacpi_error("both RSDT & XSDT tables are NULL!\n");
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    ret = initialize_from_rxsdt(rxsdt, rxsdt_entry_size);
    if (uacpi_unlikely_error(ret))
        return ret;

    ret = uacpi_namespace_initialize_predefined();
    if (uacpi_unlikely_error(ret))
        return ret;

    uacpi_install_default_address_space_handlers();

    if (params->no_acpi_mode)
        return UACPI_STATUS_OK;

    return uacpi_enter_acpi_mode();
}

struct table_load_stats {
    uacpi_u32 load_counter;
    uacpi_u32 failure_counter;
};

static void trace_table_load_failure(
    uacpi_table *tbl, enum uacpi_log_level lvl, uacpi_status ret
)
{
    uacpi_log_lvl(
        lvl,
        "failed to load '%.4s' (OEM ID '%.6s' OEM Table ID '%.8s'): %s\n",
        tbl->signature.text, tbl->hdr->oemid, tbl->hdr->oem_table_id,
        uacpi_status_to_string(ret)
    );
}

static uacpi_object_name ssdt_signature = {
    .text = { ACPI_SSDT_SIGNATURE },
};

static uacpi_object_name psdt_signature = {
    .text = { ACPI_PSDT_SIGNATURE },
};

enum uacpi_table_iteration_decision do_load_secondary_tables(
    void *user, uacpi_table *tbl
)
{
    struct table_load_stats *stats = user;
    uacpi_status ret;

    if (tbl->signature.id != ssdt_signature.id &&
        tbl->signature.id != psdt_signature.id)
        return UACPI_TABLE_ITERATION_DECISION_CONTINUE;

    ret = uacpi_load_table(tbl);
    if (uacpi_unlikely_error(ret)) {
        trace_table_load_failure(tbl, UACPI_LOG_WARN, ret);
        stats->failure_counter++;
    }
    stats->load_counter++;

    return UACPI_TABLE_ITERATION_DECISION_CONTINUE;
}

uacpi_status uacpi_namespace_load(void)
{
    struct uacpi_table *tbl;
    uacpi_status ret;
    struct table_load_stats st = { 0 };

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level !=
                       UACPI_INIT_LEVEL_TABLES_LOADED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    ret = uacpi_table_find_by_type(UACPI_TABLE_TYPE_DSDT, &tbl);
    if (uacpi_unlikely_error(ret)) {
        uacpi_error("unable to find DSDT: %s\n", uacpi_status_to_string(ret));
        return ret;
    }

    ret = uacpi_load_table(tbl);
    if (uacpi_unlikely_error(ret)) {
        trace_table_load_failure(tbl, UACPI_LOG_ERROR, ret);
        st.failure_counter++;
    }
    st.load_counter++;

    uacpi_for_each_table(0, do_load_secondary_tables, &st);

    if (uacpi_unlikely(st.failure_counter != 0)) {
        uacpi_info(
            "loaded & executed %u AML blob%s (%u error%s)\n", st.load_counter,
            st.load_counter > 1 ? "s" : "", st.failure_counter,
            st.failure_counter > 1 ? "s" : ""
        );
    } else {
        uacpi_info(
            "successfully loaded & executed %u AML blob%s\n", st.load_counter,
            st.load_counter > 1 ? "s" : ""
        );
    }

    ret = uacpi_initialize_events();
    if (uacpi_unlikely_error(ret)) {
        uacpi_warn("event initialization failed: %s\n",
                   uacpi_status_to_string(ret));
    }

    g_uacpi_rt_ctx.init_level = UACPI_INIT_LEVEL_NAMESPACE_LOADED;
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
        uacpi_memcpy(oom_absolute_path + 4, node->name.text, 4);
    }

    uacpi_warn(
        "aborted execution of '%s.%s' due to an error: %s\n",
        absolute_path, method, uacpi_status_to_string(ret)
    );

    if (uacpi_likely(absolute_path != oom_absolute_path))
        uacpi_free_dynamic_string(absolute_path);
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

    ret = uacpi_eval_sta(node, value);
    do_account_sta_ini(
        "_STA", &ctx->sta_executed, &ctx->sta_errors, node,
        *value == 0xFFFFFFFF ? UACPI_STATUS_NOT_FOUND : ret
    );

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
    uacpi_namespace_node *root;
    uacpi_address_space_handlers *handlers;
    uacpi_address_space_handler *handler;

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level !=
                       UACPI_INIT_LEVEL_NAMESPACE_LOADED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    /*
     * Initialization order here is identical to ACPICA because ACPI
     * specification doesn't really have any detailed steps that explain
     * how to do it.
     */

    root = uacpi_namespace_root();

    // Step 1 - Execute \_INI
    ini_eval(&ctx, root);

    // Step 2 - Execute \_SB._INI
    ini_eval(
        &ctx, uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB)
    );

    /*
     * Step 3 - Run _REG methods for all globally installed
     *          address space handlers.
     */
    handlers = uacpi_node_get_address_space_handlers(root);
    if (handlers) {
        handler = handlers->head;

        while (handler) {
            uacpi_reg_all_opregions(root, handler->space);
            handler = handler->next;
        }
    }

    // Step 4 - Run all other _STA and _INI methods
    uacpi_namespace_for_each_node_depth_first(root, do_sta_ini, &ctx);

    uacpi_info(
        "namespace initialization done: "
        "%zu devices, %zu thermal zones, %zu processors\n",
        ctx.devices, ctx.thermal_zones, ctx.processors
    );

    uacpi_trace(
        "_STA calls: %zu (%zu errors), _INI calls: %zu (%zu errors)\n",
        ctx.sta_executed, ctx.sta_errors, ctx.ini_executed,
        ctx.ini_errors
    );

    g_uacpi_rt_ctx.init_level = UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED;
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
        if (uacpi_likely(ret != UACPI_NULL)) {
            *ret = obj;
            uacpi_object_ref(obj);
        }

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

uacpi_status uacpi_eval_integer(
    uacpi_namespace_node *parent, const uacpi_char *path,
    uacpi_args *args, uacpi_u64 *out_value
)
{
    uacpi_object *int_obj;
    uacpi_status ret;

    ret = uacpi_eval_typed(
        parent, path, args, UACPI_OBJECT_INTEGER_BIT, &int_obj
    );
    if (uacpi_unlikely_error(ret))
        return ret;

    *out_value = int_obj->integer;
    uacpi_object_unref(int_obj);

    return UACPI_STATUS_OK;
}
