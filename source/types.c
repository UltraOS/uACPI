#include <uacpi/types.h>
#include <uacpi/internal/types.h>
#include <uacpi/internal/stdlib.h>
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

static void shareable_unref_and_delete_if_last(
    void *hdr, void (*do_free)(void*)
)
{
    if (uacpi_unlikely(bugged_shareable(hdr)))
        return;

    if (shareable_unref(hdr) == 1)
        do_free(hdr);
}

static uacpi_u32 shareable_refcount(void *hdr)
{
    struct uacpi_shareable *shareable = hdr;
    return shareable->reference_count;
}

static uacpi_bool buffer_alloc(uacpi_object *obj, uacpi_size initial_size)
{
    uacpi_buffer *buf;

    buf = uacpi_kernel_calloc(1, sizeof(uacpi_buffer));
    if (uacpi_unlikely(buf == UACPI_NULL))
        return UACPI_FALSE;

    shareable_init(buf);

    if (initial_size) {
        buf->data = uacpi_kernel_alloc(initial_size);
        if (uacpi_unlikely(buf->data == UACPI_NULL)) {
            uacpi_kernel_free(buf);
            return UACPI_FALSE;
        }

        buf->size = initial_size;
    }

    obj->buffer = buf;
    return UACPI_TRUE;
}

uacpi_object *uacpi_create_object(uacpi_object_type type)
{
    uacpi_object *ret;

    ret = uacpi_kernel_calloc(1, sizeof(*ret));
    if (uacpi_unlikely(ret == UACPI_NULL))
        return ret;

    shareable_init(ret);
    ret->type = type;

    if (type == UACPI_OBJECT_STRING || type == UACPI_OBJECT_BUFFER) {
        if (uacpi_unlikely(!buffer_alloc(ret, 0))) {
            uacpi_kernel_free(ret);
            ret = UACPI_NULL;
        }
    }

    return ret;
}

static void free_buffer(void *hdr)
{
    uacpi_buffer *buf = hdr;

    uacpi_kernel_free(buf->data);
    uacpi_kernel_free(buf);
}

static void free_object(uacpi_object *obj)
{
    if (obj->type == UACPI_OBJECT_STRING ||
        obj->type == UACPI_OBJECT_BUFFER)
        shareable_unref_and_delete_if_last(obj->buffer, free_buffer);
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

static uacpi_status buffer_alloc_and_store(
    uacpi_object *obj, uacpi_size buf_size,
    const void *src, uacpi_size src_size
)
{
    if (uacpi_unlikely(!buffer_alloc(obj, buf_size)))
        return UACPI_STATUS_OUT_OF_MEMORY;

    uacpi_memcpy_zerout(obj->buffer->data, src, buf_size, src_size);
    return UACPI_STATUS_OK;
}

static uacpi_status assign_buffer(uacpi_object *dst, uacpi_object *src,
                                  enum uacpi_assign_behavior behavior)
{
    if (behavior == UACPI_ASSIGN_BEHAVIOR_SHALLOW_COPY) {
        dst->buffer = src->buffer;
        shareable_ref(dst->buffer);
        return UACPI_STATUS_OK;
    }

    return buffer_alloc_and_store(dst, src->buffer->size,
                                  src->buffer->data, src->buffer->size);
}

uacpi_status uacpi_object_assign(uacpi_object *dst, uacpi_object *src,
                                 enum uacpi_assign_behavior behavior)
{
    uacpi_status ret = UACPI_STATUS_OK;

    if (dst->type == UACPI_OBJECT_REFERENCE) {
        uacpi_u32 refs_to_remove = shareable_refcount(dst);
        while (refs_to_remove--)
            uacpi_object_unref(dst->inner_object);
    } else if (dst->type == UACPI_OBJECT_STRING ||
               dst->type == UACPI_OBJECT_BUFFER) {
        shareable_unref_and_delete_if_last(dst->buffer, free_buffer);
    }

    switch (src->type) {
    case UACPI_OBJECT_UNINITIALIZED:
    case UACPI_OBJECT_DEBUG:
        break;
    case UACPI_OBJECT_BUFFER:
    case UACPI_OBJECT_STRING:
        ret = assign_buffer(dst, src, behavior);
        break;
    case UACPI_OBJECT_INTEGER:
        dst->integer = src->integer;
        break;
    case UACPI_OBJECT_METHOD:
        dst->method = src->method;
        break;
    case UACPI_OBJECT_REFERENCE: {
        uacpi_u32 refs_to_add = shareable_refcount(dst);

        dst->flags = src->flags;
        dst->inner_object = src->inner_object;

        while (refs_to_add-- > 0)
            uacpi_object_ref(dst->inner_object);
        break;
    } default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
    }

    if (ret == UACPI_STATUS_OK)
        dst->type = src->type;

    return ret;
}
