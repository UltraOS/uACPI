#include <uacpi/internal/notify.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/kernel_api.h>

uacpi_handlers *uacpi_node_get_handlers(
    uacpi_namespace_node *node
)
{
    uacpi_object *obj;

    obj = uacpi_namespace_node_get_object(node);
    if (uacpi_unlikely(obj == UACPI_NULL))
        return UACPI_NULL;

    switch (obj->type) {
    default:
        /*
         * Even though the '\' object doesn't have its type set to
         * UACPI_OBJECT_DEVICE, it is one.
         * See namespace.c:make_object_for_predefined for reasoning.
         */
        if (node != uacpi_namespace_root() ||
            obj->type != UACPI_OBJECT_UNINITIALIZED)
            return UACPI_NULL;
        UACPI_FALLTHROUGH;
    case UACPI_OBJECT_DEVICE:
    case UACPI_OBJECT_THERMAL_ZONE:
    case UACPI_OBJECT_PROCESSOR:
        return obj->handlers;
    }
}

/*
 * FIXME: this should probably queue the notification to be dispatched in the
 *        kernel later instead of doing it right away. On the other hand we
 *        could delegate the "dispatching" work to the kernel directly and
 *        instead keep uACPI more simple.
 */
uacpi_bool uacpi_notify_all(
    uacpi_namespace_node *node, uacpi_u64 value,
    uacpi_device_notify_handler *handler
)
{
    uacpi_bool did_notify = UACPI_FALSE;

    while (handler) {
        handler->callback(handler->user_context, node, value);
        handler = handler->next;
        did_notify = UACPI_TRUE;
    }

    return did_notify;
}

static uacpi_device_notify_handler *handler_container(
    uacpi_handlers *handlers, uacpi_notify_handler target_handler
)
{
    uacpi_device_notify_handler *handler = handlers->notify_head;

    while (handler) {
        if (handler->callback == target_handler)
            return handler;

        handler = handler->next;
    }

    return UACPI_NULL;
}

uacpi_status uacpi_install_notify_handler(
    uacpi_namespace_node *node, uacpi_notify_handler handler,
    uacpi_handle handler_context
)
{
    uacpi_handlers *handlers;
    uacpi_device_notify_handler *new_handler;

    handlers = uacpi_node_get_handlers(node);
    if (uacpi_unlikely(handlers == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;
    if (handler_container(handlers, handler) != UACPI_NULL)
        return UACPI_STATUS_ALREADY_EXISTS;

    new_handler = uacpi_kernel_calloc(1, sizeof(*new_handler));
    if (uacpi_unlikely(new_handler == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    new_handler->callback = handler;
    new_handler->user_context = handler_context;
    new_handler->next = handlers->notify_head;

    handlers->notify_head = new_handler;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_uninstall_notify_handler(
    uacpi_namespace_node *node, uacpi_notify_handler handler
)
{
    uacpi_handlers *handlers;
    uacpi_device_notify_handler *containing, *prev_handler;

    handlers = uacpi_node_get_handlers(node);
    if (uacpi_unlikely(handlers == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;

    containing = handler_container(handlers, handler);
    if (containing == UACPI_NULL)
        return UACPI_STATUS_NOT_FOUND;

    prev_handler = handlers->notify_head;

    // Are we the last linked handler?
    if (prev_handler == containing) {
        handlers->notify_head = containing->next;
        goto out;
    }

    // Nope, we're somewhere in the middle. Do a search.
    while (prev_handler) {
        if (prev_handler->next == containing) {
            prev_handler->next = containing->next;
            goto out;
        }

        prev_handler = prev_handler->next;
    }

out:
    uacpi_kernel_free(containing);
    return UACPI_STATUS_OK;
}
