#include <uacpi/internal/notify.h>
#include <uacpi/internal/shareable.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/internal/log.h>
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

struct notification_ctx {
    uacpi_namespace_node *node;
    uacpi_u64 value;
    uacpi_device_notify_handler *node_handlers, *root_handlers;
};

static void do_notify(uacpi_handle opaque)
{
    struct notification_ctx *ctx = opaque;
    uacpi_device_notify_handler *handler;
    uacpi_bool did_notify_root = UACPI_FALSE;

    handler = ctx->node_handlers;

    for (;;) {
        if (handler == UACPI_NULL) {
            if (did_notify_root) {
                uacpi_namespace_node_unref(ctx->node);
                uacpi_free(ctx, sizeof(*ctx));
                return;
            }

            handler = ctx->root_handlers;
            did_notify_root = UACPI_TRUE;
            continue;
        }

        handler->callback(handler->user_context, ctx->node, ctx->value);
        handler = handler->next;
    }
}

uacpi_status uacpi_notify_all(uacpi_namespace_node *node, uacpi_u64 value)
{
    uacpi_status ret;
    struct notification_ctx *ctx;
    uacpi_handlers *node_handlers, *root_handlers;

    node_handlers = uacpi_node_get_handlers(node);
    if (uacpi_unlikely(node_handlers == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;

    root_handlers = uacpi_node_get_handlers(uacpi_namespace_root());

    if (node_handlers->notify_head == UACPI_NULL &&
        root_handlers->notify_head == UACPI_NULL)
        return UACPI_STATUS_NO_HANDLER;

    ctx = uacpi_kernel_alloc(sizeof(*ctx));
    if (uacpi_unlikely(ctx == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    ctx->node = node;
    // In case this node goes out of scope
    uacpi_shareable_ref(node);

    ctx->value = value;
    ctx->node_handlers = node_handlers->notify_head;
    ctx->root_handlers = root_handlers->notify_head;

    ret = uacpi_kernel_schedule_work(UACPI_WORK_NOTIFICATION, do_notify, ctx);
    if (uacpi_unlikely_error(ret)) {
        uacpi_warn("unable to schedule notification work: %s\n",
                   uacpi_status_to_string(ret));
        uacpi_namespace_node_unref(node);
        uacpi_free(ctx, sizeof(*ctx));
        return ret;
    }

    return UACPI_STATUS_OK;
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
    uacpi_free(containing, sizeof(*containing));
    return UACPI_STATUS_OK;
}
