#include <uacpi/sleep.h>
#include <uacpi/internal/context.h>
#include <uacpi/internal/log.h>

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
