#pragma once

#include <uacpi/status.h>
#include <uacpi/types.h>

enum uacpi_assign_behavior {
    UACPI_ASSIGN_BEHAVIOR_DEEP_COPY,
    UACPI_ASSIGN_BEHAVIOR_SHALLOW_COPY,
};

uacpi_status uacpi_object_assign(uacpi_object *dst, uacpi_object *src,
                                 enum uacpi_assign_behavior);
