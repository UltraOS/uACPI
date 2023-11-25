#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

struct uacpi_control_method {
    struct uacpi_namespace_node *node;
    uacpi_u8 *code;
    uacpi_u32 size;
    uacpi_u8 sync_level : 4;
    uacpi_u8 args : 3;
    uacpi_u8 is_serialized : 1;
};

uacpi_status uacpi_execute_control_method(struct uacpi_control_method *method,
                                          uacpi_args *args, uacpi_object **ret);
