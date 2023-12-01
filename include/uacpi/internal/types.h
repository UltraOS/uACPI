#pragma once

#include <uacpi/status.h>
#include <uacpi/types.h>

// object->flags field if object->type == UACPI_OBJECT_REFERENCE
enum uacpi_reference_kind {
    /*
     * Stores to this reference type change the referenced object.
     * The reference is created with this kind when a RefOf result is stored
     * in an object. Detailed explanation below.
     */
    UACPI_REFERENCE_KIND_REFOF = 0,

    /*
     * Reference to a local variable, stores go into the referenced object
     * _unless_ the referenced object is a REFERENCE_KIND_REFOF. In that case,
     * the reference is unwound one more level as if the expression was
     * Store(..., DerefOf(ArgX))
     */
    UACPI_REFERENCE_KIND_LOCAL = 1,

    /*
     * Reference to an argument. Same semantics for stores as
     * REFERENCE_KIND_LOCAL.
     */
    UACPI_REFERENCE_KIND_ARG = 2,

    /*
     * Reference to a named object. Same semantics as REFERENCE_KIND_LOCAL.
     */
    UACPI_REFERENCE_KIND_NAMED = 3,
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