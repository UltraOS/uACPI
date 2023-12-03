#include <uacpi/types.h>
#include <uacpi/internal/types.h>
#include <uacpi/internal/stdlib.h>
#include <uacpi/internal/shareable.h>
#include <uacpi/kernel_api.h>

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

static uacpi_bool buffer_alloc(uacpi_object *obj, uacpi_size initial_size)
{
    uacpi_buffer *buf;

    buf = uacpi_kernel_calloc(1, sizeof(uacpi_buffer));
    if (uacpi_unlikely(buf == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(buf);

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

uacpi_bool uacpi_package_fill(uacpi_package *pkg, uacpi_size num_elements)
{
    uacpi_size i;

    pkg->objects = uacpi_kernel_calloc(num_elements, sizeof(uacpi_handle));
    if (uacpi_unlikely(pkg->objects == UACPI_NULL))
        return UACPI_FALSE;

    pkg->count = num_elements;
    for (i = 0; i < num_elements; ++i) {
        pkg->objects[i] = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);

        if (uacpi_unlikely(pkg->objects[i] == UACPI_NULL))
            return UACPI_FALSE;
    }

    return UACPI_TRUE;
}

static uacpi_bool package_alloc(uacpi_object *obj, uacpi_size initial_size)
{
    uacpi_package *pkg;

    pkg = uacpi_kernel_calloc(1, sizeof(uacpi_package));
    if (uacpi_unlikely(pkg == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(pkg);

    if (initial_size) {
        if (uacpi_unlikely(!uacpi_package_fill(pkg, initial_size))) {
            uacpi_kernel_free(pkg);
            return UACPI_FALSE;
        }
    }

    obj->package = pkg;
    return UACPI_TRUE;
}

uacpi_object *uacpi_create_object(uacpi_object_type type)
{
    uacpi_object *ret;

    ret = uacpi_kernel_calloc(1, sizeof(*ret));
    if (uacpi_unlikely(ret == UACPI_NULL))
        return ret;

    uacpi_shareable_init(ret);
    ret->type = type;

    if (type == UACPI_OBJECT_STRING || type == UACPI_OBJECT_BUFFER) {
        if (uacpi_unlikely(!buffer_alloc(ret, 0))) {
            uacpi_kernel_free(ret);
            ret = UACPI_NULL;
        }
    } else if (type == UACPI_OBJECT_PACKAGE) {
        if (uacpi_unlikely_error(!package_alloc(ret, 0))) {
            uacpi_kernel_free(ret);
            ret = UACPI_NULL;
        }
    }

    return ret;
}

static void free_buffer(uacpi_handle handle)
{
    uacpi_buffer *buf = handle;

    uacpi_kernel_free(buf->data);
    uacpi_kernel_free(buf);
}

static void free_object_storage(uacpi_object *obj)
{
    if (obj->type == UACPI_OBJECT_STRING ||
        obj->type == UACPI_OBJECT_BUFFER) {
        uacpi_shareable_unref_and_delete_if_last(obj->buffer, free_buffer);
    } else if (obj->type == UACPI_OBJECT_METHOD) {
        uacpi_kernel_free(obj->method);
    }
}

static void free_object(uacpi_object *obj)
{
    free_object_storage(obj);
    uacpi_kernel_free(obj);
}

static void make_chain_bugged(uacpi_object *obj)
{
    uacpi_kernel_log(UACPI_LOG_WARN,
                     "Object refcount bug, marking chain @%p as bugged\n",
                     obj);

    while (obj) {
        uacpi_make_shareable_bugged(obj);

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
        if (uacpi_unlikely(uacpi_bugged_shareable(obj))) {
            make_chain_bugged(this_obj);
            return;
        }

        uacpi_shareable_ref(obj);

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

        if (uacpi_shareable_refcount(obj) == 0)
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
        if (uacpi_unlikely(uacpi_bugged_shareable(obj))) {
            make_chain_bugged(this_obj);
            return;
        }

        if (uacpi_unlikely(uacpi_shareable_refcount(obj) < parent_refcount)) {
            make_chain_bugged(this_obj);
            return;
        }

        parent_refcount = uacpi_shareable_unref(obj);

        if (obj->type == UACPI_OBJECT_REFERENCE) {
            obj = obj->inner_object;
        } else {
            obj = UACPI_NULL;
        }
    }

    if (uacpi_shareable_refcount(this_obj) == 0)
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
        uacpi_shareable_ref(dst->buffer);
        return UACPI_STATUS_OK;
    }

    return buffer_alloc_and_store(dst, src->buffer->size,
                                  src->buffer->data, src->buffer->size);
}

void uacpi_object_attach_child(uacpi_object *parent, uacpi_object *child)
{
    uacpi_u32 refs_to_add;

    parent->inner_object = child;

    if (uacpi_unlikely(uacpi_bugged_shareable(parent))) {
        make_chain_bugged(child);
        return;
    }

    refs_to_add = uacpi_shareable_refcount(parent);
    while (refs_to_add--)
        uacpi_object_ref(child);
}

void uacpi_object_detach_child(uacpi_object *parent)
{
    uacpi_u32 refs_to_remove;
    uacpi_object *child;

    child = parent->inner_object;
    parent->inner_object = UACPI_NULL;

    if (uacpi_unlikely(uacpi_bugged_shareable(parent)))
        return;

    refs_to_remove = uacpi_shareable_refcount(parent);
    while (refs_to_remove--)
        uacpi_object_unref(child);
}

uacpi_status uacpi_object_assign(uacpi_object *dst, uacpi_object *src,
                                 enum uacpi_assign_behavior behavior)
{
    uacpi_status ret = UACPI_STATUS_OK;

    switch (dst->type) {
    case UACPI_OBJECT_REFERENCE:
        uacpi_object_detach_child(dst);
        break;
    case UACPI_OBJECT_STRING:
    case UACPI_OBJECT_BUFFER:
    case UACPI_OBJECT_METHOD:
        free_object_storage(dst);
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
        uacpi_object_attach_child(dst, src->inner_object);
        break;
    } default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
    }

    if (ret == UACPI_STATUS_OK)
        dst->type = src->type;

    return ret;
}

struct uacpi_object *uacpi_create_internal_reference(
    enum uacpi_reference_kind kind, uacpi_object *child
)
{
    uacpi_object *ret;

    ret = uacpi_create_object(UACPI_OBJECT_REFERENCE);
    if (uacpi_unlikely(ret == UACPI_NULL))
        return ret;

    ret->flags = kind;
    uacpi_object_attach_child(ret, child);
    return ret;
}

uacpi_object *uacpi_unwrap_internal_reference(uacpi_object *object)
{
    for (;;) {
        if (object->type != UACPI_OBJECT_REFERENCE ||
            object->flags == UACPI_REFERENCE_KIND_REFOF)
            return object;

        object = object->inner_object;
    }
}
