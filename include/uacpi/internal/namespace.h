#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

typedef struct uacpi_namespace_node {
    uacpi_object_name name;
    uacpi_object *object;
    struct uacpi_namespace_node *parent;
    struct uacpi_namespace_node *child;
    struct uacpi_namespace_node *prev;
    struct uacpi_namespace_node *next;
} uacpi_namespace_node;

uacpi_status uacpi_namespace_initialize_predefined(void);

uacpi_namespace_node *uacpi_namespace_root(void);

enum uacpi_predefined_namespace {
    UACPI_PREDEFINED_NAMESPACE_ROOT = 0,
    UACPI_PREDEFINED_NAMESPACE_GPE,
    UACPI_PREDEFINED_NAMESPACE_PR,
    UACPI_PREDEFINED_NAMESPACE_SB,
    UACPI_PREDEFINED_NAMESPACE_SI,
    UACPI_PREDEFINED_NAMESPACE_TZ,
    UACPI_PREDEFINED_NAMESPACE_GL,
    UACPI_PREDEFINED_NAMESPACE_OS,
    UACPI_PREDEFINED_NAMESPACE_OSI,
    UACPI_PREDEFINED_NAMESPACE_REV,
    UACPI_PREDEFINED_NAMESPACE_MAX = UACPI_PREDEFINED_NAMESPACE_REV,
};
uacpi_namespace_node *uacpi_namespace_get_predefined(
    enum uacpi_predefined_namespace
);

uacpi_namespace_node *uacpi_namespace_node_alloc(uacpi_object_name name);
void uacpi_namespace_node_free(uacpi_namespace_node *node);

uacpi_status uacpi_node_install(uacpi_namespace_node *parent, uacpi_namespace_node *node);
void uacpi_node_uninstall(uacpi_namespace_node *node);

uacpi_namespace_node *uacpi_namespace_node_find_sub_node(
    uacpi_namespace_node *parent,
    uacpi_object_name name
);

uacpi_object *uacpi_namespace_node_get_object(uacpi_namespace_node *node);
