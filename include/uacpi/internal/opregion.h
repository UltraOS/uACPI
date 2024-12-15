#pragma once

#include <uacpi/internal/types.h>
#include <uacpi/opregion.h>

uacpi_status uacpi_initialize_opregion(void);
void uacpi_deinitialize_opregion(void);

void uacpi_trace_region_error(
    uacpi_namespace_node *node, uacpi_char *message, uacpi_status ret
);
void uacpi_trace_region_io(
    uacpi_namespace_node *node, uacpi_address_space space, uacpi_region_op op,
    uacpi_u64 offset, uacpi_u8 byte_size, uacpi_u64 ret
);

void uacpi_opregion_uninstall_handler(uacpi_namespace_node *node);

uacpi_address_space_handlers *uacpi_node_get_address_space_handlers(
    uacpi_namespace_node *node
);

uacpi_status uacpi_initialize_opregion_node(uacpi_namespace_node *node);

uacpi_status uacpi_opregion_attach(uacpi_namespace_node *node);

void uacpi_install_default_address_space_handlers(void);

uacpi_status uacpi_dispatch_opregion_io(
    uacpi_namespace_node *region_node, uacpi_u32 offset, uacpi_u8 byte_width,
    uacpi_region_op op, uacpi_u64 *in_out
);
