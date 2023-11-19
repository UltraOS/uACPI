#include <uacpi/types.h>
#include <uacpi/kernel_api.h>

#define BUGGED_REFCOUNT 0xFFFFFFFF

const uacpi_char *uacpi_object_type_to_string(uacpi_object_type type)
{
    switch (type) {
    case UACPI_OBJECT_UNINITIALIZED:
        return "Uninitialized";
    case UACPI_OBJECT_INTEGER:
        return "Integer";
    case UACPI_OBJECT_STRING:
        return "String";
    case UACPI_OBJECT_BUFFER:
        return "Buffer";
    case UACPI_OBJECT_PACKAGE:
        return "Package";
    case UACPI_OBJECT_REFERENCE:
        return "Reference";
    case UACPI_OBJECT_METHOD:
        return "Method";
    case UACPI_OBJECT_DEBUG:
        return "Debug";
    default:
        return "<Invalid type>";
    }
}

uacpi_object *uacpi_create_object(uacpi_object_type type)
{
    uacpi_object *ret;

    ret = uacpi_kernel_calloc(1, sizeof(*ret));
    if (uacpi_unlikely(ret == UACPI_NULL))
        return ret;

    ret->common.refcount = 1;
    ret->type = type;
    return ret;
}

static void free_object(uacpi_object *obj)
{
    if (obj->type == UACPI_OBJECT_STRING ||
        obj->type == UACPI_OBJECT_BUFFER)
        uacpi_kernel_free(obj->as_buffer.data);
    if (obj->type == UACPI_OBJECT_METHOD)
        uacpi_kernel_free(obj->as_method.method);

    uacpi_kernel_free(obj);
}

static uacpi_bool bugged_object(uacpi_object *obj)
{
    uacpi_object_common *common = &obj->common;

    if (uacpi_unlikely(common->refcount == 0))
        common->refcount = BUGGED_REFCOUNT;

    return common->refcount == BUGGED_REFCOUNT;
}

void make_chain_bugged(uacpi_object *obj)
{
    uacpi_kernel_log(UACPI_LOG_WARN,
                     "Object refcount bug, marking chain @%p as bugged\n",
                     obj);

    while (obj) {
        obj->common.refcount = BUGGED_REFCOUNT;

        if (obj->type == UACPI_OBJECT_REFERENCE)
            obj = obj->as_reference.object;
        else
            obj = UACPI_NULL;
    }
}

void uacpi_object_ref(uacpi_object *obj)
{
    uacpi_object *this_obj = obj;

    while (obj) {
        if (uacpi_unlikely(bugged_object(obj))) {
            make_chain_bugged(this_obj);
            return;
        }

        obj->common.refcount++;
        if (obj->type == UACPI_OBJECT_REFERENCE)
            obj = obj->as_reference.object;
        else
            obj = UACPI_NULL;
    }
}

static void free_chain(uacpi_object *obj)
{
    uacpi_object *next_obj = UACPI_NULL;

    while (obj) {
        if (obj->type == UACPI_OBJECT_REFERENCE)
            next_obj = obj->as_reference.object;

        if (obj->common.refcount == 0)
            free_object(obj);

        obj = next_obj;
        next_obj = UACPI_NULL;
    }
}

void uacpi_object_unref(uacpi_object *obj)
{
    uacpi_object *this_obj = obj;
    uacpi_u32 parent_refcount;
    uacpi_object_common *common;

    if (!obj)
        return;

    parent_refcount = obj->common.refcount;

    while (obj) {
        common = &obj->common;

        if (uacpi_unlikely(bugged_object(obj))) {
            make_chain_bugged(this_obj);
            return;
        }

        if (uacpi_unlikely(common->refcount < parent_refcount)) {
            make_chain_bugged(this_obj);
            return;
        }

        parent_refcount = common->refcount--;

        if (obj->type == UACPI_OBJECT_REFERENCE) {
            obj = obj->as_reference.object;
        } else {
            obj = UACPI_NULL;
        }
    }

    if (this_obj->common.refcount == 0)
        free_chain(this_obj);
}
