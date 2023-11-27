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

static void shareable_init(void *hdr)
{
    struct uacpi_shareable *shareable = hdr;
    shareable->reference_count = 1;
}

static uacpi_bool bugged_shareable(void *hdr)
{
    struct uacpi_shareable *shareable = hdr;

    if (uacpi_unlikely(shareable->reference_count == 0))
        shareable->reference_count = BUGGED_REFCOUNT;

    return shareable->reference_count == BUGGED_REFCOUNT;
}

static void make_shareable_bugged(void *hdr)
{
    struct uacpi_shareable *shareable = hdr;
    shareable->reference_count = BUGGED_REFCOUNT;
}

static uacpi_u32 shareable_ref(void *hdr)
{
    struct uacpi_shareable *shareable = hdr;
    return shareable->reference_count++;
}

static uacpi_u32 shareable_unref(void *hdr)
{
    struct uacpi_shareable *shareable = hdr;
    return shareable->reference_count--;
}

static uacpi_u32 shareable_refcount(void *hdr)
{
    struct uacpi_shareable *shareable = hdr;
    return shareable->reference_count;
}

uacpi_object *uacpi_create_object(uacpi_object_type type)
{
    uacpi_object *ret;

    ret = uacpi_kernel_calloc(1, sizeof(*ret));
    if (uacpi_unlikely(ret == UACPI_NULL))
        return ret;

    shareable_init(ret);
    ret->type = type;
    return ret;
}

static void free_object(uacpi_object *obj)
{
    if (obj->type == UACPI_OBJECT_STRING ||
        obj->type == UACPI_OBJECT_BUFFER)
        uacpi_kernel_free(obj->buffer.data);
    if (obj->type == UACPI_OBJECT_METHOD)
        uacpi_kernel_free(obj->method);

    uacpi_kernel_free(obj);
}

void make_chain_bugged(uacpi_object *obj)
{
    uacpi_kernel_log(UACPI_LOG_WARN,
                     "Object refcount bug, marking chain @%p as bugged\n",
                     obj);

    while (obj) {
        make_shareable_bugged(obj);

        if (obj->type == UACPI_OBJECT_REFERENCE)
            obj = obj->inner_object;
        else
            obj = UACPI_NULL;
    }
}

void uacpi_object_ref(uacpi_object *obj)
{
    uacpi_object *this_obj = obj;

    while (obj) {
        if (uacpi_unlikely(bugged_shareable(obj))) {
            make_chain_bugged(this_obj);
            return;
        }

        shareable_ref(obj);

        if (obj->type == UACPI_OBJECT_REFERENCE)
            obj = obj->inner_object;
        else
            obj = UACPI_NULL;
    }
}

static void free_chain(uacpi_object *obj)
{
    uacpi_object *next_obj = UACPI_NULL;

    while (obj) {
        if (obj->type == UACPI_OBJECT_REFERENCE)
            next_obj = obj->inner_object;

        if (shareable_refcount(obj) == 0)
            free_object(obj);

        obj = next_obj;
        next_obj = UACPI_NULL;
    }
}

void uacpi_object_unref(uacpi_object *obj)
{
    uacpi_object *this_obj = obj;
    uacpi_u32 parent_refcount;

    if (!obj)
        return;

    parent_refcount = obj->shareable.reference_count;

    while (obj) {
        if (uacpi_unlikely(bugged_shareable(obj))) {
            make_chain_bugged(this_obj);
            return;
        }

        if (uacpi_unlikely(shareable_refcount(obj) < parent_refcount)) {
            make_chain_bugged(this_obj);
            return;
        }

        parent_refcount = shareable_unref(obj);

        if (obj->type == UACPI_OBJECT_REFERENCE) {
            obj = obj->inner_object;
        } else {
            obj = UACPI_NULL;
        }
    }

    if (shareable_refcount(this_obj) == 0)
        free_chain(this_obj);
}
