#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/internal/tables.h>

uacpi_status uacpi_load_table(struct uacpi_table*);
uacpi_status uacpi_osi(uacpi_handle handle, uacpi_object *retval);

uacpi_status uacpi_execute_control_method(uacpi_namespace_node *scope,
                                          uacpi_control_method *method,
                                          uacpi_args *args, uacpi_object **ret);
