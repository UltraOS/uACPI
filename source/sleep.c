#include <uacpi/sleep.h>
#include <uacpi/internal/context.h>
#include <uacpi/internal/log.h>
#include <uacpi/internal/io.h>
#include <uacpi/internal/registers.h>
#include <uacpi/platform/arch_helpers.h>

static uacpi_status get_slp_type_for_state(uacpi_u8 state)
{
    uacpi_char path[] = "_S0";
    uacpi_status ret;
    uacpi_object *arg, *obj0, *obj1, *ret_obj = UACPI_NULL;
    uacpi_args args;

    arg = uacpi_create_object(UACPI_OBJECT_INTEGER);
    if (uacpi_unlikely(arg == UACPI_NULL)) {
        ret = UACPI_STATUS_OUT_OF_MEMORY;
        goto out;
    }

    path[2] += state;
    arg->integer = state;
    args.objects = &arg;
    args.count = 1;

    ret = uacpi_eval_typed(
        uacpi_namespace_root(), path, &args,
        UACPI_OBJECT_PACKAGE_BIT, &ret_obj
    );
    if (ret != UACPI_STATUS_OK) {
        if (uacpi_unlikely(ret != UACPI_STATUS_NOT_FOUND)) {
            uacpi_warn("error while evaluating %s: %s\n", path,
                       uacpi_status_to_string(ret));
        } else {
            uacpi_trace("sleep state %d is not supported as %s was not found\n",
                        state, path);
        }
        goto out;
    }

    switch (ret_obj->package->count) {
    case 0:
        uacpi_error("empty package while evaluating %s!\n", path);
        ret = UACPI_STATUS_AML_INCOMPATIBLE_OBJECT_TYPE;
        goto out;

    case 1:
        obj0 = ret_obj->package->objects[0];
        if (uacpi_unlikely(obj0->type != UACPI_OBJECT_INTEGER)) {
            uacpi_error(
                "invalid object type at pkg[0] => %s when evaluating %s\n",
                uacpi_object_type_to_string(obj0->type), path
            );
            goto out;
        }

        g_uacpi_rt_ctx.last_sleep_typ_a = obj0->integer;
        g_uacpi_rt_ctx.last_sleep_typ_b = obj0->integer >> 8;
        break;

    default:
        obj0 = ret_obj->package->objects[0];
        obj1 = ret_obj->package->objects[1];

        if (uacpi_unlikely(obj0->type != UACPI_OBJECT_INTEGER ||
                           obj1->type != UACPI_OBJECT_INTEGER)) {
            uacpi_error(
                "invalid object type when evaluating %s: "
                "pkg[0] => %s, pkg[1] => %s\n", path,
                uacpi_object_type_to_string(obj0->type),
                uacpi_object_type_to_string(obj1->type)
            );
            ret = UACPI_STATUS_AML_INCOMPATIBLE_OBJECT_TYPE;
            goto out;
        }

        g_uacpi_rt_ctx.last_sleep_typ_a = obj0->integer;
        g_uacpi_rt_ctx.last_sleep_typ_b = obj1->integer;
        break;
    }

out:
    if (ret != UACPI_STATUS_OK) {
        g_uacpi_rt_ctx.last_sleep_typ_a = UACPI_SLEEP_TYP_INVALID;
        g_uacpi_rt_ctx.last_sleep_typ_b = UACPI_SLEEP_TYP_INVALID;
    }

    uacpi_object_unref(arg);
    uacpi_object_unref(ret_obj);
    return ret;
}

static uacpi_status eval_sleep_helper(
    uacpi_namespace_node *parent, const uacpi_char *path, uacpi_u8 value
)
{
    uacpi_object *arg;
    uacpi_args args;
    uacpi_status ret;

    arg = uacpi_create_object(UACPI_OBJECT_INTEGER);
    if (uacpi_unlikely(arg == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    arg->integer = value;
    args.objects = &arg;
    args.count = 1;

    ret = uacpi_eval(parent, path, &args, UACPI_NULL);
    switch (ret) {
    case UACPI_STATUS_OK:
        break;
    case UACPI_STATUS_NOT_FOUND:
        ret = UACPI_STATUS_OK;
        break;
    default:
        uacpi_error("error while evaluating %s: %s\n",
                    path, uacpi_status_to_string(ret));
        break;
    }

    uacpi_object_unref(arg);
    return ret;
}

static uacpi_status eval_pts(uacpi_u8 state)
{
    return eval_sleep_helper(uacpi_namespace_root(), "_PTS", state);
}

static uacpi_status eval_sst_for_state(enum uacpi_sleep_state state)
{
    uacpi_u8 arg;

    /*
     * This optional object is a control method that OSPM invokes to set the
     * system status indicator as desired.
     * Arguments:(1)
     * Arg0 - An Integer containing the system status indicator identifier:
     *     0 - No system state indication. Indicator off
     *     1 - Working
     *     2 - Waking
     *     3 - Sleeping. Used to indicate system state S1, S2, or S3
     *     4 - Sleeping with context saved to non-volatile storage
     */
    switch (state) {
    case UACPI_SLEEP_STATE_S0:
        arg = 1;
        break;
    case UACPI_SLEEP_STATE_S1:
    case UACPI_SLEEP_STATE_S2:
    case UACPI_SLEEP_STATE_S3:
        arg = 3;
        break;
    case UACPI_SLEEP_STATE_S4:
        arg = 4;
        break;
    case UACPI_SLEEP_STATE_S5:
        arg = 0;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return eval_sleep_helper(
        uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SI),
        "_SST", arg
    );
}

uacpi_status uacpi_prepare_for_sleep_state(enum uacpi_sleep_state state_enum)
{
    uacpi_u8 state = state_enum;
    uacpi_status ret;

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level !=
                       UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;
    if (uacpi_unlikely(state > UACPI_SLEEP_STATE_S5))
        return UACPI_STATUS_INVALID_ARGUMENT;

    ret = get_slp_type_for_state(state);
    if (ret != UACPI_STATUS_OK)
        return ret;

    ret = eval_pts(state);
    if (uacpi_unlikely_error(ret))
        return ret;

    eval_sst_for_state(state);
    return UACPI_STATUS_OK;
}

static uacpi_u8 make_hw_reduced_sleep_control(void)
{
    uacpi_u8 value;

    value = (g_uacpi_rt_ctx.last_sleep_typ_a << ACPI_SLP_CNT_SLP_TYP_IDX);
    value &= ACPI_SLP_CNT_SLP_TYP_MASK;
    value |= ACPI_SLP_CNT_SLP_EN_MASK;

    return value;
}

static uacpi_status enter_hw_reduced_sleep_state(uacpi_u8 state)
{
    uacpi_status ret;
    uacpi_u8 sleep_control;
    uacpi_u64 wake_status;
    struct acpi_fadt *fadt = &g_uacpi_rt_ctx.fadt;

    if (!fadt->sleep_control_reg.address || !fadt->sleep_status_reg.address)
        return UACPI_STATUS_NOT_FOUND;

    ret = uacpi_write_register_field(
        UACPI_REGISTER_FIELD_HWR_WAK_STS,
        ACPI_SLP_STS_CLEAR
    );
    if (uacpi_unlikely_error(ret))
        return ret;

    sleep_control = make_hw_reduced_sleep_control();

    if (state < UACPI_SLEEP_STATE_S4)
        UACPI_ARCH_FLUSH_CPU_CACHE();

    /*
     * To put the system into a sleep state, software will write the HW-reduced
     * Sleep Type value (obtained from the \_Sx object in the DSDT) and the
     * SLP_EN bit to the sleep control register.
     */
    ret = uacpi_write_register(UACPI_REGISTER_SLP_CNT, sleep_control);
    if (uacpi_unlikely_error(ret))
        return ret;

    /*
     * The OSPM then polls the WAK_STS bit of the SLEEP_STATUS_REG waiting for
     * it to be one (1), indicating that the system has been transitioned
     * back to the Working state.
     */
    do {
        ret = uacpi_read_register_field(
            UACPI_REGISTER_FIELD_HWR_WAK_STS, &wake_status
        );
        if (uacpi_unlikely_error(ret))
            return ret;
    } while (wake_status != 1);

    return UACPI_STATUS_OK;
}

static uacpi_status enter_sleep_state(uacpi_u8 state)
{
    uacpi_status ret;
    uacpi_u64 wake_status, pm1a, pm1b;

    ret = uacpi_write_register_field(
        UACPI_REGISTER_FIELD_WAK_STS, ACPI_PM1_STS_CLEAR
    );
    if (uacpi_unlikely_error(ret))
        return ret;

    ret = uacpi_read_register(UACPI_REGISTER_PM1_CNT, &pm1a);
    if (uacpi_unlikely_error(ret))
        return ret;

    pm1a &= ~((uacpi_u64)(ACPI_PM1_CNT_SLP_TYP_MASK | ACPI_PM1_CNT_SLP_EN_MASK));
    pm1b = pm1a;

    pm1a |= g_uacpi_rt_ctx.last_sleep_typ_a << ACPI_PM1_CNT_SLP_TYP_IDX;
    pm1b |= g_uacpi_rt_ctx.last_sleep_typ_b << ACPI_PM1_CNT_SLP_TYP_IDX;

    /*
     * Just like ACPICA, split writing SLP_TYP and SLP_EN to work around
     * buggy firmware that can't handle both written at the same time.
     */
    ret = uacpi_write_registers(UACPI_REGISTER_PM1_CNT, pm1a, pm1b);
    if (uacpi_unlikely_error(ret))
        return ret;

    pm1a |= ACPI_PM1_CNT_SLP_EN_MASK;
    pm1b |= ACPI_PM1_CNT_SLP_EN_MASK;

    if (state < UACPI_SLEEP_STATE_S4)
        UACPI_ARCH_FLUSH_CPU_CACHE();

    ret = uacpi_write_registers(UACPI_REGISTER_PM1_CNT, pm1a, pm1b);
    if (uacpi_unlikely_error(ret))
        return ret;

    if (state > UACPI_SLEEP_STATE_S3) {
        /*
         * We're still here, this is a bug or very slow firmware.
         * Just try spinning for a bit.
         */
        uacpi_u64 stalled_time = 0;

        // 10 seconds max
        while (stalled_time < (10 * 1000 * 1000)) {
            uacpi_kernel_stall(100);
            stalled_time += 100;
        }

        // Try one more time
        ret = uacpi_write_registers(UACPI_REGISTER_PM1_CNT, pm1a, pm1b);
        if (uacpi_unlikely_error(ret))
            return ret;

        // Nothing we can do here, give up
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    do {
        ret = uacpi_read_register_field(
            UACPI_REGISTER_FIELD_WAK_STS, &wake_status
        );
        if (uacpi_unlikely_error(ret))
            return ret;
    } while (wake_status != 1);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_enter_sleep_state(enum uacpi_sleep_state state_enum)
{
    uacpi_u8 state = state_enum;

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level !=
                       UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;
    if (uacpi_unlikely(state > UACPI_SLEEP_STATE_MAX))
        return UACPI_STATUS_INVALID_ARGUMENT;

    if (uacpi_unlikely(g_uacpi_rt_ctx.last_sleep_typ_a > ACPI_SLP_TYP_MAX ||
                       g_uacpi_rt_ctx.last_sleep_typ_b > ACPI_SLP_TYP_MAX)) {
        uacpi_error("invalid SLP_TYP values: 0x%02X:0x%02X\n",
                    g_uacpi_rt_ctx.last_sleep_typ_a,
                    g_uacpi_rt_ctx.last_sleep_typ_b);
        return UACPI_STATUS_AML_BAD_ENCODING;
    }

    if (g_uacpi_rt_ctx.is_hardware_reduced)
        return enter_hw_reduced_sleep_state(state);

    return enter_sleep_state(state);
}
