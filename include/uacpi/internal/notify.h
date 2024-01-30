#pragma once

#include <uacpi/notify.h>

uacpi_bool uacpi_notify_all(
    uacpi_namespace_node *node, uacpi_u64 value,
    uacpi_device_notify_handler *handler
);

uacpi_handlers *uacpi_node_get_handlers(
    uacpi_namespace_node *node
);
