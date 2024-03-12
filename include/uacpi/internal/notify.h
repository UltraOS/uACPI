#pragma once

#include <uacpi/notify.h>

uacpi_status uacpi_notify_all(uacpi_namespace_node *node, uacpi_u64 value);

uacpi_handlers *uacpi_node_get_handlers(
    uacpi_namespace_node *node
);
