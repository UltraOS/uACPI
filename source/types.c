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

static uacpi_status assign_buffer(uacpi_object *dst, uacpi_object *src,
                                  enum uacpi_assign_behavior behavior)
{
    uacpi_buffer *src_buf = &src->buffer;
    uacpi_buffer *dst_buf = &dst->buffer;

    if (behavior == UACPI_ASSIGN_BEHAVIOR_MOVE ||
       (behavior == UACPI_ASSIGN_BEHAVIOR_MOVE_IF_POSSIBLE &&
        src->shareable.reference_count == 1)) {
        dst_buf->data = src_buf->data;
        dst_buf->size = src_buf->size;
        src_buf->data = UACPI_NULL;
        src_buf->size = 0;
    } else {
        dst_buf->data = uacpi_kernel_alloc(src_buf->size);
        if (uacpi_unlikely(dst_buf->data == UACPI_NULL))
            return UACPI_STATUS_OUT_OF_MEMORY;

        dst_buf->size = src_buf->size;
        uacpi_memcpy(dst_buf->data, src_buf->data, src_buf->size);
    }

    return UACPI_STATUS_OK;
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
        uacpi_kernel_free(dst->buffer.data);
        dst->buffer.data = NULL;
        dst->buffer.size = 0;
    }

    if (behavior == UACPI_ASSIGN_BEHAVIOR_MOVE &&
        uacpi_unlikely(shareable_refcount(src) != 1)) {
        uacpi_kernel_log(UACPI_LOG_WARN,
                         "Tried to move an object (%p) with %u references, "
                         "converting to copy\n", src,
                         src->shareable.reference_count);
        behavior = UACPI_ASSIGN_BEHAVIOR_COPY;
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
