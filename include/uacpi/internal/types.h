#pragma once

#include <uacpi/status.h>
#include <uacpi/types.h>

// object->flags field if object->type == UACPI_OBJECT_REFERENCE
enum uacpi_reference_kind {
    UACPI_REFERENCE_KIND_REFOF = 0,
    UACPI_REFERENCE_KIND_LOCAL = 1,
    UACPI_REFERENCE_KIND_ARG = 2,
    UACPI_REFERENCE_KIND_NAMED = 3,
    UACPI_REFERENCE_KIND_PKG_INDEX = 4,
};

// object->flags field if object->type == UACPI_OBJECT_STRING
enum uacpi_string_kind {
    UACPI_STRING_KIND_NORMAL = 0,
    UACPI_STRING_KIND_PATH,
};

/*
 * TODO: Write a note here explaining how references are currently implemented
 *       and how some of the edge cases are handled.
 */

enum uacpi_assign_behavior {
    UACPI_ASSIGN_BEHAVIOR_DEEP_COPY,
    UACPI_ASSIGN_BEHAVIOR_SHALLOW_COPY,
};

uacpi_status uacpi_object_assign(uacpi_object *dst, uacpi_object *src,
                                 enum uacpi_assign_behavior);

void uacpi_object_attach_child(uacpi_object *parent, uacpi_object *child);
void uacpi_object_detach_child(uacpi_object *parent);

struct uacpi_object *uacpi_create_internal_reference(
    enum uacpi_reference_kind kind, uacpi_object *child
);
uacpi_object *uacpi_unwrap_internal_reference(uacpi_object *object);

uacpi_bool uacpi_package_fill(uacpi_package *pkg, uacpi_size num_elements);

uacpi_mutex *uacpi_create_mutex(void);
void uacpi_mutex_unref(uacpi_mutex*);

void uacpi_address_space_handler_unref(uacpi_address_space_handler *handler);
