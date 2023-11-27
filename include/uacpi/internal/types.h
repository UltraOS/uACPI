#pragma once

#include <uacpi/status.h>
#include <uacpi/types.h>

enum uacpi_assign_behavior {
    // Deep copy
    UACPI_ASSIGN_BEHAVIOR_COPY,

    // Shallow copy
    UACPI_ASSIGN_BEHAVIOR_MOVE,

    // Attempt to shallow copy with fallback
    UACPI_ASSIGN_BEHAVIOR_MOVE_IF_POSSIBLE,
};

uacpi_status uacpi_object_assign(uacpi_object *dst, uacpi_object *src,
                                 enum uacpi_assign_behavior);
