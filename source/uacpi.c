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
#include <uacpi/internal/osi.h>

struct uacpi_runtime_context g_uacpi_rt_ctx = { 0 };

void uacpi_state_reset(void)
{
    uacpi_deinitialize_namespace();
    uacpi_deinitialize_interfaces();
    uacpi_deinitialize_events();
    uacpi_deinitialize_tables();

#ifndef UACPI_REDUCED_HARDWARE
    if (g_uacpi_rt_ctx.global_lock_event)
        uacpi_kernel_free_event(g_uacpi_rt_ctx.global_lock_event);
    if (g_uacpi_rt_ctx.global_lock_spinlock)
        uacpi_kernel_free_spinlock(g_uacpi_rt_ctx.global_lock_spinlock);
#endif

    uacpi_memzero(&g_uacpi_rt_ctx, sizeof(g_uacpi_rt_ctx));

#ifdef UACPI_KERNEL_INITIALIZATION
    uacpi_kernel_deinitialize();
#endif
}

void uacpi_context_set_log_level(uacpi_log_level lvl)
{
    if (lvl == 0)
        lvl = UACPI_DEFAULT_LOG_LEVEL;

    g_uacpi_rt_ctx.log_level = lvl;
}

void uacpi_context_set_loop_timeout(uacpi_u32 seconds)
{
    if (seconds == 0)
        seconds = UACPI_DEFAULT_LOOP_TIMEOUT_SECONDS;

    g_uacpi_rt_ctx.loop_timeout_seconds = seconds;
}

void uacpi_context_set_max_call_stack_depth(uacpi_u32 depth)
{
    if (depth == 0)
        depth = UACPI_DEFAULT_MAX_CALL_STACK_DEPTH;

    g_uacpi_rt_ctx.max_call_stack_depth = depth;
}

uacpi_u32 uacpi_context_get_loop_timeout(void)
{
    return g_uacpi_rt_ctx.loop_timeout_seconds;
}

void uacpi_context_set_proactive_table_checksum(uacpi_bool setting)
{
    if (setting)
        g_uacpi_rt_ctx.flags |= UACPI_FLAG_PROACTIVE_TBL_CSUM;
    else
        g_uacpi_rt_ctx.flags &= ~UACPI_FLAG_PROACTIVE_TBL_CSUM;
}

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
    case UACPI_STATUS_TIMEOUT:
        return "wait timed out";
    case UACPI_STATUS_OVERRIDDEN:
        return "the requested action has been overridden";
    case UACPI_STATUS_DENIED:
        return "the requested action has been denied";

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
    case UACPI_STATUS_AML_LOOP_TIMEOUT:
        return "hanging AML while loop";
    case UACPI_STATUS_AML_CALL_STACK_DEPTH_LIMIT:
        return "reached maximum AML call stack depth";
    default:
        return "<invalid status>";
    }
}

#ifndef UACPI_REDUCED_HARDWARE
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

    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED);

    if (uacpi_is_hardware_reduced())
        return UACPI_STATUS_OK;

    mode_str = mode == HW_MODE_LEGACY ? "legacy" : "acpi";

    if (read_mode() == mode) {
        uacpi_trace("%s mode already enabled\n", mode_str);
        return UACPI_STATUS_OK;
    }

    ret = set_mode(mode);
    if (uacpi_unlikely_error(ret)) {
        uacpi_warn(
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

uacpi_init_level uacpi_get_current_init_level(void)
{
    return g_uacpi_rt_ctx.init_level;
}

uacpi_status uacpi_initialize(uacpi_u64 flags)
{
    uacpi_status ret;

    UACPI_ENSURE_INIT_LEVEL_IS(UACPI_INIT_LEVEL_EARLY);

#ifdef UACPI_KERNEL_INITIALIZATION
    ret = uacpi_kernel_initialize(UACPI_INIT_LEVEL_EARLY);
    if (uacpi_unlikely_error(ret))
        return ret;
#endif

    g_uacpi_rt_ctx.init_level = UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED;
    g_uacpi_rt_ctx.last_sleep_typ_a = UACPI_SLEEP_TYP_INVALID;
    g_uacpi_rt_ctx.last_sleep_typ_b = UACPI_SLEEP_TYP_INVALID;
    g_uacpi_rt_ctx.s0_sleep_typ_a = UACPI_SLEEP_TYP_INVALID;
    g_uacpi_rt_ctx.s0_sleep_typ_b = UACPI_SLEEP_TYP_INVALID;
    g_uacpi_rt_ctx.flags = flags;

    if (g_uacpi_rt_ctx.log_level == 0)
        uacpi_context_set_log_level(UACPI_DEFAULT_LOG_LEVEL);
    if (g_uacpi_rt_ctx.loop_timeout_seconds == 0)
        uacpi_context_set_loop_timeout(UACPI_DEFAULT_LOOP_TIMEOUT_SECONDS);
    if (g_uacpi_rt_ctx.max_call_stack_depth == 0)
        uacpi_context_set_max_call_stack_depth(UACPI_DEFAULT_MAX_CALL_STACK_DEPTH);

    ret = uacpi_initialize_tables();
    if (uacpi_unlikely_error(ret))
        goto out_fatal_error;

    ret = uacpi_initialize_interfaces();
    if (uacpi_unlikely_error(ret))
        goto out_fatal_error;

    ret = uacpi_initialize_namespace();
    if (uacpi_unlikely_error(ret))
        goto out_fatal_error;

    uacpi_install_default_address_space_handlers();

    if (!uacpi_check_flag(UACPI_FLAG_NO_ACPI_MODE)) {
        // This is not critical, so just ignore the return status
        uacpi_enter_acpi_mode();
    }
    return UACPI_STATUS_OK;

out_fatal_error:
    uacpi_state_reset();
    return ret;
}

struct table_load_stats {
    uacpi_u32 load_counter;
    uacpi_u32 failure_counter;
};

static void trace_table_load_failure(
    struct acpi_sdt_hdr *tbl, uacpi_log_level lvl, uacpi_status ret
)
{
    uacpi_log_lvl(
        lvl,
        "failed to load '%.4s' (OEM ID '%.6s' OEM Table ID '%.8s'): %s\n",
        tbl->signature, tbl->oemid, tbl->oem_table_id,
        uacpi_status_to_string(ret)
    );
}

static uacpi_bool match_ssdt_or_psdt(struct uacpi_installed_table *tbl)
{
    if (tbl->flags & UACPI_TABLE_LOADED)
        return UACPI_FALSE;

    return uacpi_signatures_match(tbl->hdr.signature, ACPI_SSDT_SIGNATURE) ||
           uacpi_signatures_match(tbl->hdr.signature, ACPI_PSDT_SIGNATURE);
}

uacpi_status uacpi_namespace_load(void)
{
    struct uacpi_table tbl;
    uacpi_status ret;
    struct table_load_stats st = { 0 };
    uacpi_size cur_index;

    UACPI_ENSURE_INIT_LEVEL_IS(UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED);

#ifdef UACPI_KERNEL_INITIALIZATION
    ret = uacpi_kernel_initialize(UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED);
    if (uacpi_unlikely_error(ret))
        goto out_fatal_error;
#endif

    ret = uacpi_table_find_by_signature(ACPI_DSDT_SIGNATURE, &tbl);
    if (uacpi_unlikely_error(ret)) {
        uacpi_error("unable to find DSDT: %s\n", uacpi_status_to_string(ret));
        goto out_fatal_error;
    }

    ret = uacpi_table_load_with_cause(tbl.index, UACPI_TABLE_LOAD_CAUSE_INIT);
    if (uacpi_unlikely_error(ret)) {
        trace_table_load_failure(tbl.hdr, UACPI_LOG_ERROR, ret);
        st.failure_counter++;
    }
    st.load_counter++;
    uacpi_table_unref(&tbl);

    for (cur_index = 0;; cur_index = tbl.index + 1) {
        ret = uacpi_table_match(cur_index, match_ssdt_or_psdt, &tbl);
        if (ret != UACPI_STATUS_OK) {
            if (uacpi_unlikely(ret != UACPI_STATUS_NOT_FOUND))
                return ret;

            break;
        }

        ret = uacpi_table_load_with_cause(tbl.index, UACPI_TABLE_LOAD_CAUSE_INIT);
        if (uacpi_unlikely_error(ret)) {
            trace_table_load_failure(tbl.hdr, UACPI_LOG_WARN, ret);
            st.failure_counter++;
        }
        st.load_counter++;
        uacpi_table_unref(&tbl);
    }

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
        goto out_fatal_error;
    }

    g_uacpi_rt_ctx.init_level = UACPI_INIT_LEVEL_NAMESPACE_LOADED;
    return UACPI_STATUS_OK;

out_fatal_error:
    uacpi_state_reset();
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

static void ini_eval(struct ns_init_context *ctx, uacpi_namespace_node *node)
{
    uacpi_status ret;

    ret = uacpi_eval(node, "_INI", UACPI_NULL, UACPI_NULL);
    if (ret == UACPI_STATUS_NOT_FOUND)
        return;

    ctx->ini_executed++;
    if (uacpi_unlikely_error(ret))
        ctx->ini_errors++;
}

static uacpi_status sta_eval(
    struct ns_init_context *ctx, uacpi_namespace_node *node,
    uacpi_u32 *value
)
{
    uacpi_status ret;

    ret = uacpi_eval_sta(node, value);
    if (*value == 0xFFFFFFFF)
        return ret;

    ctx->sta_executed++;
    if (uacpi_unlikely_error(ret))
        ctx->sta_errors++;

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
    uacpi_status ret = UACPI_STATUS_OK;

    UACPI_ENSURE_INIT_LEVEL_IS(UACPI_INIT_LEVEL_NAMESPACE_LOADED);

#ifdef UACPI_KERNEL_INITIALIZATION
    ret = uacpi_kernel_initialize(UACPI_INIT_LEVEL_NAMESPACE_LOADED);
    if (uacpi_unlikely_error(ret))
        goto out;
#endif

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
#ifdef UACPI_KERNEL_INITIALIZATION
    ret = uacpi_kernel_initialize(UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED);
out:
    if (uacpi_unlikely_error(ret))
        uacpi_state_reset();
#endif
    return ret;
}

uacpi_status
uacpi_eval(uacpi_namespace_node *parent, const uacpi_char *path,
           const uacpi_args *args, uacpi_object **ret)
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

#define TRACE_BAD_RET(path_fmt, type, ...)                                 \
    uacpi_warn(                                                            \
        "unexpected '%s' object returned by method "path_fmt               \
        ", expected type mask: %08X\n", uacpi_object_type_to_string(type), \
        __VA_ARGS__                                                        \
    )

#define TRACE_NO_RET(path_fmt, ...)                                        \
    uacpi_warn(                                                            \
        "no value returned from method "path_fmt", expected type mask: "   \
        "%08X\n", __VA_ARGS__                                              \
    )

static void trace_invalid_return_type(
    uacpi_namespace_node *parent, const uacpi_char *path,
    uacpi_u32 expected_mask, uacpi_object_type actual_type
)
{
    const uacpi_char *abs_path;
    uacpi_bool dynamic_abs_path = UACPI_FALSE;

    if (parent == UACPI_NULL || (path != UACPI_NULL && path[0] == '\\')) {
        abs_path = path;
    } else {
        abs_path = uacpi_namespace_node_generate_absolute_path(parent);
        dynamic_abs_path = UACPI_TRUE;
    }

    if (dynamic_abs_path && path != UACPI_NULL) {
        if (actual_type == UACPI_OBJECT_UNINITIALIZED)
            TRACE_NO_RET("%s.%s", abs_path, path, expected_mask);
        else
            TRACE_BAD_RET("%s.%s", actual_type, abs_path, path, expected_mask);
    } else {
        if (actual_type == UACPI_OBJECT_UNINITIALIZED) {
            TRACE_NO_RET("%s", abs_path, expected_mask);
        } else {
            TRACE_BAD_RET("%s", actual_type, abs_path, expected_mask);
        }
    }

    if (dynamic_abs_path)
        uacpi_free_dynamic_string(abs_path);
}

uacpi_status uacpi_eval_typed(
    uacpi_namespace_node *parent, const uacpi_char *path,
    const uacpi_args *args, uacpi_u32 ret_mask, uacpi_object **out_obj
)
{
    uacpi_status ret;
    uacpi_object *obj;
    uacpi_object_type returned_type = UACPI_OBJECT_UNINITIALIZED;

    if (uacpi_unlikely(out_obj == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;

    ret = uacpi_eval(parent, path, args, &obj);
    if (uacpi_unlikely_error(ret))
        return ret;

    if (obj != UACPI_NULL)
        returned_type = obj->type;

    if (ret_mask && (ret_mask & (1 << returned_type)) == 0) {
        trace_invalid_return_type(parent, path, ret_mask, returned_type);
        uacpi_object_unref(obj);
        return UACPI_STATUS_TYPE_MISMATCH;
    }

    *out_obj = obj;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_eval_integer(
    uacpi_namespace_node *parent, const uacpi_char *path,
    const uacpi_args *args, uacpi_u64 *out_value
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
