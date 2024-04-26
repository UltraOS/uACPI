#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>
#include <uacpi/namespace.h>

#define UACPI_NAMESPACE_NODE_FLAG_ALIAS (1 << 0)

/*
 * This node has been uninstalled and has no object associated with it.
 *
 * This is used to handle edge cases where an object needs to reference
 * a namespace node, where the node might end up going out of scope before
 * the object lifetime ends.
 */
#define UACPI_NAMESPACE_NODE_FLAG_DANGLING (1 << 1)

typedef struct uacpi_namespace_node {
    struct uacpi_shareable shareable;
    uacpi_object_name name;
    uacpi_u32 flags;
    uacpi_object *object;
    struct uacpi_namespace_node *parent;
    struct uacpi_namespace_node *child;
    struct uacpi_namespace_node *next;
} uacpi_namespace_node;

uacpi_status uacpi_namespace_initialize_predefined(void);

uacpi_namespace_node *uacpi_namespace_node_alloc(uacpi_object_name name);
void uacpi_namespace_node_unref(uacpi_namespace_node *node);

uacpi_status uacpi_node_install(uacpi_namespace_node *parent, uacpi_namespace_node *node);
void uacpi_node_uninstall(uacpi_namespace_node *node);

uacpi_namespace_node *uacpi_namespace_node_find_sub_node(
    uacpi_namespace_node *parent,
    uacpi_object_name name
);

uacpi_bool uacpi_namespace_node_is_dangling(uacpi_namespace_node *node);
