#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

struct uacpi_control_method {
    uacpi_object_type type;
    uacpi_u8 *code;
    uacpi_u32 size;
    uacpi_u8 args : 3;
    uacpi_u8 sync_level : 4;
    bool is_serialized : 1;
};

uacpi_status uacpi_execute_control_method(struct uacpi_control_method *method,
                                          uacpi_args *args, uacpi_object **ret);
