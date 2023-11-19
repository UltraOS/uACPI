#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

struct uacpi_namespace_node {
    uacpi_object_name name;
    uacpi_object *object;
    struct uacpi_namespace_node *parent;
    struct uacpi_namespace_node *child;
    struct uacpi_namespace_node *next;
};

struct uacpi_namespace_node *uacpi_namespace_node_alloc(uacpi_object_name name,
                                                        uacpi_object_type type);
void uacpi_namespace_node_free(struct uacpi_namespace_node* node);

uacpi_status uacpi_node_install(struct uacpi_namespace_node *parent, struct uacpi_namespace_node *node);

struct uacpi_namespace_node *uacpi_namespace_node_find(
    struct uacpi_namespace_node *parent,
    uacpi_object_name name
);
