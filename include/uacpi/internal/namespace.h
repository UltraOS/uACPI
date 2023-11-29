#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

typedef struct uacpi_namespace_node {
    uacpi_object_name name;
    uacpi_object *object;
    struct uacpi_namespace_node *parent;
    struct uacpi_namespace_node *child;
    struct uacpi_namespace_node *next;
} uacpi_namespace_node;

uacpi_namespace_node *uacpi_namespace_node_alloc(uacpi_object_name name,
                                                 uacpi_object_type type);
void uacpi_namespace_node_free(uacpi_namespace_node *node);

uacpi_status uacpi_node_install(uacpi_namespace_node *parent, uacpi_namespace_node *node);

uacpi_namespace_node *uacpi_namespace_node_find(
    uacpi_namespace_node *parent,
    uacpi_object_name name
);
