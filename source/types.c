#include <uacpi/types.h>
#include <uacpi/internal/types.h>
#include <uacpi/internal/stdlib.h>
#include <uacpi/internal/shareable.h>
#include <uacpi/internal/dynamic_array.h>
#include <uacpi/internal/log.h>
#include <uacpi/internal/namespace.h>
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
    case UACPI_OBJECT_FIELD_UNIT:
        return "Field Unit";
    case UACPI_OBJECT_DEVICE:
        return "Device";
    case UACPI_OBJECT_EVENT:
        return "Event";
    case UACPI_OBJECT_REFERENCE:
        return "Reference";
    case UACPI_OBJECT_BUFFER_INDEX:
        return "Buffer Index";
    case UACPI_OBJECT_METHOD:
        return "Method";
    case UACPI_OBJECT_MUTEX:
        return "Mutex";
    case UACPI_OBJECT_OPERATION_REGION:
        return "Operation Region";
    case UACPI_OBJECT_POWER_RESOURCE:
        return "Power Resource";
    case UACPI_OBJECT_PROCESSOR:
        return "Processor";
    case UACPI_OBJECT_THERMAL_ZONE:
        return "Thermal Zone";
    case UACPI_OBJECT_DEBUG:
        return "Debug";
    default:
        return "<Invalid type>";
    }
}

const uacpi_char *uacpi_address_space_to_string(
    enum uacpi_address_space space
)
{
    switch (space) {
    case UACPI_ADDRESS_SPACE_SYSTEM_MEMORY:
        return "SystemMemory";
    case UACPI_ADDRESS_SPACE_SYSTEM_IO:
        return "SystemIO";
    case UACPI_ADDRESS_SPACE_PCI_CONFIG:
        return "PCI_Config";
    case UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER:
        return "EmbeddedControl";
    case UACPI_ADDRESS_SPACE_SMBUS:
        return "SMBus";
    case UACPI_ADDRESS_SPACE_SYSTEM_CMOS:
        return "SystemCMOS";
    case UACPI_ADDRESS_SPACE_PCI_BAR_TARGET:
        return "PciBarTarget";
    case UACPI_ADDRESS_SPACE_IPMI:
        return "IPMI";
    case UACPI_ADDRESS_SPACE_GENERAL_PURPOSE_IO:
        return "GeneralPurposeIO";
    case UACPI_ADDRESS_SPACE_GENERIC_SERIAL_BUS:
        return "GenericSerialBus";
    case UACPI_ADDRESS_SPACE_PCC:
        return "PCC";
    case UACPI_ADDRESS_SPACE_PRM:
        return "PRM";
    case UACPI_ADDRESS_SPACE_FFIXEDHW:
        return "FFixedHW";
    case UACPI_ADDRESS_SPACE_TABLE_DATA:
        return "TableData";
    default:
        return "<vendor specific>";
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
            uacpi_free(buf, sizeof(*buf));
            return UACPI_FALSE;
        }

        buf->size = initial_size;
    }

    obj->buffer = buf;
    return UACPI_TRUE;
}

static uacpi_bool empty_buffer_or_string_alloc(uacpi_object *object)
{
    return buffer_alloc(object, 0);
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
            uacpi_free(pkg, sizeof(*pkg));
            return UACPI_FALSE;
        }
    }

    obj->package = pkg;
    return UACPI_TRUE;
}

static uacpi_bool empty_package_alloc(uacpi_object *object)
{
    return package_alloc(object, 0);
}

uacpi_mutex *uacpi_create_mutex(void)
{
    uacpi_mutex *mutex;

    mutex = uacpi_kernel_calloc(1, sizeof(uacpi_mutex));
    if (uacpi_unlikely(mutex == UACPI_NULL))
        return UACPI_NULL;

    mutex->handle = uacpi_kernel_create_mutex();
    if (mutex->handle == UACPI_NULL) {
        uacpi_free(mutex, sizeof(*mutex));
        return UACPI_NULL;
    }

    uacpi_shareable_init(mutex);
    return mutex;
}

static uacpi_bool mutex_alloc(uacpi_object *obj)
{
    obj->mutex = uacpi_create_mutex();
    return obj->mutex != UACPI_NULL;
}

static uacpi_bool event_alloc(uacpi_object *obj)
{
    uacpi_event *event;

    event = uacpi_kernel_calloc(1, sizeof(uacpi_event));
    if (uacpi_unlikely(event == UACPI_NULL))
        return UACPI_FALSE;

    event->handle = uacpi_kernel_create_event();
    if (event->handle == UACPI_NULL) {
        uacpi_free(event, sizeof(*event));
        return UACPI_FALSE;
    }

    uacpi_shareable_init(event);
    obj->event = event;

    return UACPI_TRUE;
}

static uacpi_bool method_alloc(uacpi_object *obj)
{
    uacpi_control_method *method;

    method = uacpi_kernel_calloc(1, sizeof(*method));
    if (uacpi_unlikely(method == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(method);
    obj->method = method;

    return UACPI_TRUE;
}

static uacpi_bool op_region_alloc(uacpi_object *obj)
{
    uacpi_operation_region *op_region;

    op_region = uacpi_kernel_calloc(1, sizeof(*op_region));
    if (uacpi_unlikely(op_region == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(op_region);
    obj->op_region = op_region;

    return UACPI_TRUE;
}

static uacpi_bool field_unit_alloc(uacpi_object *obj)
{
    uacpi_field_unit *field_unit;

    field_unit = uacpi_kernel_calloc(1, sizeof(*field_unit));
    if (uacpi_unlikely(field_unit == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(field_unit);
    obj->field_unit = field_unit;

    return UACPI_TRUE;
}

static uacpi_bool processor_alloc(uacpi_object *obj)
{
    uacpi_processor *processor;

    processor = uacpi_kernel_calloc(1, sizeof(*processor));
    if (uacpi_unlikely(processor == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(processor);
    obj->processor = processor;

    return UACPI_TRUE;
}

static uacpi_bool device_alloc(uacpi_object *obj)
{
    uacpi_device *device;

    device = uacpi_kernel_calloc(1, sizeof(*device));
    if (uacpi_unlikely(device == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(device);
    obj->device = device;

    return UACPI_TRUE;
}

static uacpi_bool thermal_zone_alloc(uacpi_object *obj)
{
    uacpi_thermal_zone *thermal_zone;

    thermal_zone = uacpi_kernel_calloc(1, sizeof(*thermal_zone));
    if (uacpi_unlikely(thermal_zone == UACPI_NULL))
        return UACPI_FALSE;

    uacpi_shareable_init(thermal_zone);
    obj->thermal_zone = thermal_zone;

    return UACPI_TRUE;
}

typedef uacpi_bool (*object_ctor)(uacpi_object *obj);

static object_ctor object_constructor_table[UACPI_OBJECT_MAX_TYPE_VALUE + 1] = {
    [UACPI_OBJECT_STRING] = empty_buffer_or_string_alloc,
    [UACPI_OBJECT_BUFFER] = empty_buffer_or_string_alloc,
    [UACPI_OBJECT_PACKAGE] = empty_package_alloc,
    [UACPI_OBJECT_FIELD_UNIT] = field_unit_alloc,
    [UACPI_OBJECT_MUTEX] = mutex_alloc,
    [UACPI_OBJECT_EVENT] = event_alloc,
    [UACPI_OBJECT_OPERATION_REGION] = op_region_alloc,
    [UACPI_OBJECT_METHOD] = method_alloc,
    [UACPI_OBJECT_PROCESSOR] = processor_alloc,
    [UACPI_OBJECT_DEVICE] = device_alloc,
    [UACPI_OBJECT_THERMAL_ZONE] = thermal_zone_alloc,
};

uacpi_object *uacpi_create_object(uacpi_object_type type)
{
    uacpi_object *ret;
    object_ctor ctor;

    ret = uacpi_kernel_calloc(1, sizeof(*ret));
    if (uacpi_unlikely(ret == UACPI_NULL))
        return ret;

    uacpi_shareable_init(ret);
    ret->type = type;

    ctor = object_constructor_table[type];
    if (ctor == UACPI_NULL)
        return ret;

    if (uacpi_unlikely(!ctor(ret))) {
        uacpi_free(ret, sizeof(*ret));
        return UACPI_NULL;
    }

    return ret;
}

static void free_buffer(uacpi_handle handle)
{
    uacpi_buffer *buf = handle;

    if (buf->data != UACPI_NULL)
        /*
         * If buffer has a size of 0 but a valid data pointer it's probably an
         * "empty" buffer allocated by the interpreter in make_null_buffer
         * and its real size is actually 1.
         */
        uacpi_free(buf->data, UACPI_MAX(buf->size, 1));

    uacpi_free(buf, sizeof(*buf));
}

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(free_queue, uacpi_package*, 4)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(free_queue, uacpi_package*, static)

static uacpi_bool free_queue_push(struct free_queue *queue, uacpi_package *pkg)
{
    uacpi_package **slot;

    slot = free_queue_alloc(queue);
    if (uacpi_unlikely(slot == UACPI_NULL))
        return UACPI_FALSE;

    *slot = pkg;
    return UACPI_TRUE;
}

static void free_object(uacpi_object *obj);

// No references allowed here, only plain objects
static void free_plain_no_recurse(uacpi_object *obj, struct free_queue *queue)
{
    switch (obj->type) {
    case UACPI_OBJECT_PACKAGE:
        if (uacpi_shareable_unref(obj->package) > 1)
            break;

        if (uacpi_unlikely(!free_queue_push(queue,
                                            obj->package))) {
            uacpi_warn(
                "unable to free nested package @%p: not enough memory\n",
                obj->package
            );
        }

        // Don't call free_object here as that will recurse
        uacpi_free(obj, sizeof(*obj));
        break;
    default:
        /*
         * This call is guaranteed to not recurse further as we handle
         * recursive cases elsewhere explicitly.
         */
        free_object(obj);
    }
}

static void unref_plain_no_recurse(uacpi_object *obj, struct free_queue *queue)
{
    if (uacpi_shareable_unref(obj) > 1)
        return;

    free_plain_no_recurse(obj, queue);
}

static void unref_chain_no_recurse(uacpi_object *obj, struct free_queue *queue)
{
    uacpi_object *next_obj = UACPI_NULL;

    while (obj) {
        if (obj->type == UACPI_OBJECT_REFERENCE)
            next_obj = obj->inner_object;

        if (uacpi_shareable_unref(obj) > 1)
            goto do_next;

        if (obj->type == UACPI_OBJECT_REFERENCE) {
            uacpi_free(obj, sizeof(*obj));
        } else {
            free_plain_no_recurse(obj, queue);
        }

    do_next:
        obj = next_obj;
        next_obj = UACPI_NULL;
    }
}

static void unref_object_no_recurse(uacpi_object *obj, struct free_queue *queue)
{
    if (obj->type == UACPI_OBJECT_REFERENCE) {
        unref_chain_no_recurse(obj, queue);
        return;
    }

    unref_plain_no_recurse(obj, queue);
}

static void free_package(uacpi_handle handle)
{
    struct free_queue queue = { 0 };
    uacpi_package *pkg = handle;
    uacpi_object *obj;
    uacpi_size i;

    free_queue_push(&queue, pkg);

    while (free_queue_size(&queue) != 0) {
        pkg = *free_queue_last(&queue);
        free_queue_pop(&queue);

        /*
         * 1. Unref/free every object in the package. Note that this might add
         *    even more packages into the free queue.
         */
        for (i = 0; i < pkg->count; ++i) {
            obj = pkg->objects[i];
            unref_object_no_recurse(obj, &queue);
        }

        // 2. Release the object array
        uacpi_free(pkg->objects, sizeof(*pkg->objects) * pkg->count);

        // 3. Release the package itself
        uacpi_free(pkg, sizeof(*pkg));
    }

    free_queue_clear(&queue);
}

static void free_mutex(uacpi_handle handle)
{
    uacpi_mutex *mutex = handle;

    uacpi_kernel_free_mutex(mutex->handle);
    uacpi_free(mutex, sizeof(*mutex));
}

void uacpi_mutex_unref(uacpi_mutex *mutex)
{
    if (mutex == UACPI_NULL)
        return;

    uacpi_shareable_unref_and_delete_if_last(mutex, free_mutex);
}

static void free_event(uacpi_handle handle)
{
    uacpi_event *event = handle;

    uacpi_kernel_free_event(event->handle);
    uacpi_free(event, sizeof(*event));
}

static void free_address_space_handler(uacpi_handle handle)
{
    uacpi_address_space_handler *handler = handle;
    uacpi_free(handler, sizeof(*handler));
}

static void free_address_space_handlers(
    uacpi_address_space_handler *handler
)
{
    uacpi_address_space_handler *next_handler;

    while (handler) {
        next_handler = handler->next;
        uacpi_shareable_unref_and_delete_if_last(
            handler, free_address_space_handler
        );
        handler = next_handler;
    }
}

static void free_device_notify_handlers(uacpi_device_notify_handler *handler)
{
    uacpi_device_notify_handler *next_handler;

    while (handler) {
        next_handler = handler->next;
        uacpi_free(handler, sizeof(*handler));
        handler = next_handler;
    }
}

static void free_handlers(uacpi_handle handle)
{
    uacpi_handlers *handlers = handle;

    free_address_space_handlers(handlers->address_space_head);
    free_device_notify_handlers(handlers->notify_head);
}

void uacpi_address_space_handler_unref(uacpi_address_space_handler *handler)
{
    uacpi_shareable_unref_and_delete_if_last(
        handler, free_address_space_handler
    );
}

static void free_op_region(uacpi_handle handle)
{
    uacpi_operation_region *op_region = handle;

    if (uacpi_unlikely(op_region->handler != UACPI_NULL)) {
        uacpi_warn(
            "BUG: attempting to free an opregion@%p with a handler attached\n",
            op_region
        );
    }

    uacpi_free(op_region, sizeof(*op_region));
}

static void free_device(uacpi_handle handle)
{
    uacpi_device *device = handle;
    free_handlers(device);
    uacpi_free(device, sizeof(*device));
}

static void free_processor(uacpi_handle handle)
{
    uacpi_processor *processor = handle;
    free_handlers(processor);
    uacpi_free(processor, sizeof(*processor));
}

static void free_thermal_zone(uacpi_handle handle)
{
    uacpi_thermal_zone *thermal_zone = handle;
    free_handlers(thermal_zone);
    uacpi_free(thermal_zone, sizeof(*thermal_zone));
}

static void free_field_unit(uacpi_handle handle)
{
    uacpi_field_unit *field_unit = handle;

    switch (field_unit->kind) {
    case UACPI_FIELD_UNIT_KIND_NORMAL:
        uacpi_namespace_node_unref(field_unit->region);
        break;
    case UACPI_FIELD_UNIT_KIND_BANK:
        uacpi_namespace_node_unref(field_unit->bank_region);
        break;
    case UACPI_FIELD_UNIT_KIND_INDEX:
        uacpi_shareable_unref_and_delete_if_last(
            field_unit->index, free_field_unit
        );
        uacpi_shareable_unref_and_delete_if_last(
            field_unit->data, free_field_unit
        );
        break;
    default:
        break;
    }

    uacpi_free(field_unit, sizeof(*field_unit));
}

static void free_method(uacpi_handle handle)
{
    uacpi_control_method *method = handle;

    uacpi_shareable_unref_and_delete_if_last(
        method->mutex, free_mutex
    );

    uacpi_free(method, sizeof(*method));
}

static void free_object_storage(uacpi_object *obj)
{
    switch (obj->type) {
    case UACPI_OBJECT_STRING:
    case UACPI_OBJECT_BUFFER:
        uacpi_shareable_unref_and_delete_if_last(obj->buffer, free_buffer);
        break;
    case UACPI_OBJECT_BUFFER_FIELD:
        uacpi_shareable_unref_and_delete_if_last(obj->buffer_field.backing,
                                                 free_buffer);
        break;
    case UACPI_OBJECT_BUFFER_INDEX:
        uacpi_shareable_unref_and_delete_if_last(obj->buffer_index.buffer,
                                                 free_buffer);
        break;
    case UACPI_OBJECT_METHOD:
        uacpi_shareable_unref_and_delete_if_last(obj->method,
                                                 free_method);
        break;
    case UACPI_OBJECT_PACKAGE:
        uacpi_shareable_unref_and_delete_if_last(obj->package,
                                                 free_package);
        break;
    case UACPI_OBJECT_FIELD_UNIT:
        uacpi_shareable_unref_and_delete_if_last(obj->field_unit,
                                                 free_field_unit);
        break;
    case UACPI_OBJECT_MUTEX:
        uacpi_mutex_unref(obj->mutex);
        break;
    case UACPI_OBJECT_EVENT:
        uacpi_shareable_unref_and_delete_if_last(obj->event,
                                                 free_event);
        break;
    case UACPI_OBJECT_OPERATION_REGION:
        uacpi_shareable_unref_and_delete_if_last(obj->op_region,
                                                 free_op_region);
        break;
    case UACPI_OBJECT_PROCESSOR:
        uacpi_shareable_unref_and_delete_if_last(obj->processor,
                                                 free_processor);
        break;
    case UACPI_OBJECT_DEVICE:
        uacpi_shareable_unref_and_delete_if_last(obj->device,
                                                 free_device);
        break;
    case UACPI_OBJECT_THERMAL_ZONE:
        uacpi_shareable_unref_and_delete_if_last(obj->thermal_zone,
                                                 free_thermal_zone);
        break;
    default:
        break;
    }
}

static void free_object(uacpi_object *obj)
{
    free_object_storage(obj);
    uacpi_free(obj, sizeof(*obj));
}

static void make_chain_bugged(uacpi_object *obj)
{
    uacpi_warn("object refcount bug, marking chain @%p as bugged\n", obj);

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

struct pkg_copy_req {
    uacpi_object *dst;
    uacpi_package *src;
};

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(pkg_copy_reqs, struct pkg_copy_req, 2)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(
    pkg_copy_reqs, struct pkg_copy_req, static
)

static uacpi_bool pkg_copy_reqs_push(
    struct pkg_copy_reqs *reqs,
    uacpi_object *dst, uacpi_package *pkg
)
{
    struct pkg_copy_req *req;

    req = pkg_copy_reqs_alloc(reqs);
    if (uacpi_unlikely(req == UACPI_NULL))
        return UACPI_FALSE;

    req->dst = dst;
    req->src = pkg;

    return UACPI_TRUE;
}

static uacpi_status deep_copy_package_no_recurse(
    uacpi_object *dst, uacpi_package *src,
    struct pkg_copy_reqs *reqs
)
{
    uacpi_size i;
    uacpi_package *dst_package;

    if (uacpi_unlikely(!package_alloc(dst, src->count)))
        return UACPI_STATUS_OUT_OF_MEMORY;

    dst->type = UACPI_OBJECT_PACKAGE;
    dst_package = dst->package;

    for (i = 0; i < src->count; ++i) {
        uacpi_status st;
        uacpi_object *src_obj = src->objects[i];
        uacpi_object *dst_obj = dst_package->objects[i];

        // Don't copy the internal package index reference
        if (src_obj->type == UACPI_OBJECT_REFERENCE &&
            src_obj->flags == UACPI_REFERENCE_KIND_PKG_INDEX)
            src_obj = src_obj->inner_object;

        if (src_obj->type == UACPI_OBJECT_PACKAGE) {
            uacpi_bool ret;

            ret = pkg_copy_reqs_push(reqs, dst_obj, src_obj->package);
            if (uacpi_unlikely(!ret))
                return UACPI_STATUS_OUT_OF_MEMORY;

            continue;
        }

        st = uacpi_object_assign(dst_obj, src_obj,
                                 UACPI_ASSIGN_BEHAVIOR_DEEP_COPY);
        if (uacpi_unlikely_error(st))
            return st;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status deep_copy_package(uacpi_object *dst, uacpi_object *src)
{
    uacpi_status ret = UACPI_STATUS_OK;
    struct pkg_copy_reqs reqs = { 0 };

    pkg_copy_reqs_push(&reqs, dst, src->package);

    while (pkg_copy_reqs_size(&reqs) != 0) {
        struct pkg_copy_req req;

        req = *pkg_copy_reqs_last(&reqs);
        pkg_copy_reqs_pop(&reqs);

        ret = deep_copy_package_no_recurse(req.dst, req.src, &reqs);
        if (uacpi_unlikely_error(ret))
            break;
    }

    pkg_copy_reqs_clear(&reqs);
    return ret;
}

static uacpi_status assign_mutex(uacpi_object *dst, uacpi_object *src,
                                 enum uacpi_assign_behavior behavior)
{
    if (behavior == UACPI_ASSIGN_BEHAVIOR_DEEP_COPY) {
        if (uacpi_likely(mutex_alloc(dst))) {
            dst->mutex->sync_level = src->mutex->sync_level;
            return UACPI_STATUS_OK;
        }

        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    dst->mutex = src->mutex;
    uacpi_shareable_ref(dst->mutex);

    return UACPI_STATUS_OK;
}

static uacpi_status assign_event(uacpi_object *dst, uacpi_object *src,
                                 enum uacpi_assign_behavior behavior)
{
    if (behavior == UACPI_ASSIGN_BEHAVIOR_DEEP_COPY) {
        if (uacpi_likely(event_alloc(dst)))
            return UACPI_STATUS_OK;

        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    dst->event = src->event;
    uacpi_shareable_ref(dst->event);

    return UACPI_STATUS_OK;
}

static uacpi_status assign_package(uacpi_object *dst, uacpi_object *src,
                                   enum uacpi_assign_behavior behavior)
{
    if (behavior == UACPI_ASSIGN_BEHAVIOR_SHALLOW_COPY) {
        dst->package = src->package;
        uacpi_shareable_ref(dst->package);
        return UACPI_STATUS_OK;
    }

    return deep_copy_package(dst, src);
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

    if (src == dst)
        return ret;

    switch (dst->type) {
    case UACPI_OBJECT_REFERENCE:
        uacpi_object_detach_child(dst);
        break;
    case UACPI_OBJECT_STRING:
    case UACPI_OBJECT_BUFFER:
    case UACPI_OBJECT_METHOD:
    case UACPI_OBJECT_PACKAGE:
    case UACPI_OBJECT_MUTEX:
    case UACPI_OBJECT_EVENT:
    case UACPI_OBJECT_PROCESSOR:
    case UACPI_OBJECT_DEVICE:
    case UACPI_OBJECT_THERMAL_ZONE:
        free_object_storage(dst);
        break;
    default:
        break;
    }

    switch (src->type) {
    case UACPI_OBJECT_UNINITIALIZED:
    case UACPI_OBJECT_DEBUG:
        break;
    case UACPI_OBJECT_BUFFER:
    case UACPI_OBJECT_STRING:
        dst->flags = src->flags;
        ret = assign_buffer(dst, src, behavior);
        break;
    case UACPI_OBJECT_BUFFER_FIELD:
        dst->buffer_field = src->buffer_field;
        uacpi_shareable_ref(dst->buffer_field.backing);
        break;
    case UACPI_OBJECT_BUFFER_INDEX:
        dst->buffer_index = src->buffer_index;
        uacpi_shareable_ref(dst->buffer_index.buffer);
        break;
    case UACPI_OBJECT_INTEGER:
        dst->integer = src->integer;
        break;
    case UACPI_OBJECT_METHOD:
        dst->method = src->method;
        uacpi_shareable_ref(dst->method);
        break;
    case UACPI_OBJECT_MUTEX:
        ret = assign_mutex(dst, src, behavior);
        break;
    case UACPI_OBJECT_EVENT:
        ret = assign_event(dst, src, behavior);
        break;
    case UACPI_OBJECT_OPERATION_REGION:
        dst->op_region = src->op_region;
        uacpi_shareable_ref(dst->op_region);
        break;
    case UACPI_OBJECT_PACKAGE:
        ret = assign_package(dst, src, behavior);
        break;
    case UACPI_OBJECT_FIELD_UNIT:
        dst->field_unit = src->field_unit;
        uacpi_shareable_ref(dst->field_unit);
        break;
    case UACPI_OBJECT_REFERENCE:
        uacpi_object_attach_child(dst, src->inner_object);
        break;
    case UACPI_OBJECT_PROCESSOR:
        dst->processor = src->processor;
        uacpi_shareable_ref(dst->processor);
        break;
    case UACPI_OBJECT_DEVICE:
        dst->device = src->device;
        uacpi_shareable_ref(dst->device);
        break;
    case UACPI_OBJECT_THERMAL_ZONE:
        dst->thermal_zone = src->thermal_zone;
        uacpi_shareable_ref(dst->thermal_zone);
        break;
    default:
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
            (object->flags == UACPI_REFERENCE_KIND_REFOF ||
             object->flags == UACPI_REFERENCE_KIND_PKG_INDEX))
            return object;

        object = object->inner_object;
    }
}
