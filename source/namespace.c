#include <uacpi/namespace.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/internal/types.h>
#include <uacpi/internal/stdlib.h>
#include <uacpi/internal/interpreter.h>
#include <uacpi/kernel_api.h>

#define UACPI_REV_VALUE 2
#define UACPI_OS_VALUE "Microsoft Windows NT"

uacpi_namespace_node
predefined_namespaces[UACPI_PREDEFINED_NAMESPACE_MAX + 1] = {
    [UACPI_PREDEFINED_NAMESPACE_ROOT] = { .name.text = "\\" },
    [UACPI_PREDEFINED_NAMESPACE_GPE] = { .name.text = "_GPE" },
    [UACPI_PREDEFINED_NAMESPACE_PR] = { .name.text = "_PR_" },
    [UACPI_PREDEFINED_NAMESPACE_SB] = { .name.text = "_SB_" },
    [UACPI_PREDEFINED_NAMESPACE_SI] = { .name.text = "_SI_" },
    [UACPI_PREDEFINED_NAMESPACE_TZ] = { .name.text = "_TZ_" },
    [UACPI_PREDEFINED_NAMESPACE_GL] = { .name.text = "_GL_" },
    [UACPI_PREDEFINED_NAMESPACE_OS] = { .name.text = "_OS_" },
    [UACPI_PREDEFINED_NAMESPACE_OSI] = { .name.text = "_OSI" },
    [UACPI_PREDEFINED_NAMESPACE_REV] = { .name.text = "_REV" },
};

static uacpi_object *make_object_for_predefined(
    enum uacpi_predefined_namespace ns
)
{
    uacpi_object *obj;

    switch (ns) {
    case UACPI_PREDEFINED_NAMESPACE_OS:
        obj = uacpi_create_object(UACPI_OBJECT_STRING);
        if (uacpi_unlikely(obj == UACPI_NULL))
            return obj;

        obj->buffer->text = uacpi_kernel_alloc(sizeof(UACPI_OS_VALUE));
        if (uacpi_unlikely(obj->buffer->text == UACPI_NULL)) {
            uacpi_object_unref(obj);
            return UACPI_NULL;
        }

        obj->buffer->size = sizeof(UACPI_OS_VALUE);
        uacpi_memcpy(obj->buffer->text, UACPI_OS_VALUE, obj->buffer->size);
        break;

    case UACPI_PREDEFINED_NAMESPACE_REV:
        obj = uacpi_create_object(UACPI_OBJECT_INTEGER);
        if (uacpi_unlikely(obj == UACPI_NULL))
            return obj;

        obj->integer = UACPI_REV_VALUE;
        break;

    case UACPI_PREDEFINED_NAMESPACE_GL:
        obj = uacpi_create_object(UACPI_OBJECT_MUTEX);
        break;

    case UACPI_PREDEFINED_NAMESPACE_OSI:
        obj = uacpi_create_object(UACPI_OBJECT_METHOD);
        if (uacpi_unlikely(obj == UACPI_NULL))
            return obj;

        obj->method->native_call = UACPI_TRUE;
        obj->method->handler = uacpi_osi;
        obj->method->args = 1;
        break;

    default:
        obj = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);
        break;
    }

    return obj;
}

uacpi_status uacpi_namespace_initialize_predefined(void)
{
    enum uacpi_predefined_namespace ns;
    uacpi_object *obj;
    uacpi_namespace_node *node;

    for (ns = 0; ns <= UACPI_PREDEFINED_NAMESPACE_MAX; ns++) {
        node = &predefined_namespaces[ns];

        obj = make_object_for_predefined(ns);
        if (uacpi_unlikely(obj == UACPI_NULL))
            return UACPI_STATUS_OUT_OF_MEMORY;

        node->object = uacpi_create_internal_reference(
            UACPI_REFERENCE_KIND_NAMED, obj
        );
        if (uacpi_unlikely(node->object == UACPI_NULL)) {
            uacpi_object_unref(obj);
            return UACPI_STATUS_OUT_OF_MEMORY;
        }
    }

    for (ns = UACPI_PREDEFINED_NAMESPACE_GPE;
         ns <= UACPI_PREDEFINED_NAMESPACE_MAX; ns++)
        uacpi_node_install(uacpi_namespace_root(), &predefined_namespaces[ns]);

    return UACPI_STATUS_OK;
}

uacpi_namespace_node *uacpi_namespace_root(void)
{
    return &predefined_namespaces[UACPI_PREDEFINED_NAMESPACE_ROOT];
}

uacpi_namespace_node *uacpi_namespace_get_predefined(
    enum uacpi_predefined_namespace ns
)
{
    if (uacpi_unlikely(ns > UACPI_PREDEFINED_NAMESPACE_MAX)) {
        uacpi_kernel_log(
            UACPI_LOG_WARN, "Requested invalid predefined namespace %d\n",
            ns
        );
        return UACPI_NULL;
    }

    return &predefined_namespaces[ns];
}

uacpi_namespace_node *uacpi_namespace_node_alloc(uacpi_object_name name)
{
    uacpi_namespace_node *ret;

    ret = uacpi_kernel_calloc(1, sizeof(*ret));
    if (uacpi_unlikely(ret == UACPI_NULL))
        return ret;

    ret->name = name;
    return ret;
}

void uacpi_namespace_node_free(uacpi_namespace_node *node)
{
    if (node == UACPI_NULL)
        return;
    if (node->object)
        uacpi_object_unref(node->object);

    uacpi_kernel_free(node);
}

uacpi_status uacpi_node_install(
    uacpi_namespace_node *parent,
    uacpi_namespace_node *node
)
{
    uacpi_namespace_node *prev;

    if (parent == UACPI_NULL)
        parent = uacpi_namespace_root();

    prev = parent->child;
    parent->child = node;
    node->parent = parent;
    node->next = prev;

    if (prev) {
        if (uacpi_unlikely(prev->prev != UACPI_NULL)) {
            uacpi_kernel_log(
                UACPI_LOG_WARN,
                "while installing node @p: previous node @p already has "
                " a valid prev link @p\n", node, prev, prev->prev
            );
        }

        prev->prev = node;
    }

    return UACPI_STATUS_OK;
}

void uacpi_node_uninstall(uacpi_namespace_node *node)
{
    if (node->parent && node->parent->child == node)
        node->parent->child = node->next;

    if (node->prev)
        node->prev->next = node->next;
    if (node->next)
        node->next->prev = node->prev;

    if (uacpi_unlikely(node->child)) {
        uacpi_kernel_log(
            UACPI_LOG_WARN,
            "trying to uninstall a node @p with a valid child link @p\n",
            node, node->child
        );
    }

    uacpi_namespace_node_free(node);
}

uacpi_namespace_node *uacpi_namespace_node_find_sub_node(
    uacpi_namespace_node *parent,
    uacpi_object_name name
)
{
    if (parent == UACPI_NULL)
        parent = uacpi_namespace_root();

    uacpi_namespace_node *node = parent->child;

    while (node) {
        if (node->name.id == name.id)
            return node;

        node = node->next;
    }

    return UACPI_NULL;
}

uacpi_namespace_node *uacpi_namespace_node_find(
    uacpi_namespace_node *parent,
    const uacpi_char *path
)
{
    uacpi_namespace_node *cur_node = parent;
    const uacpi_char *cursor = path;
    uacpi_size bytes_left;
    uacpi_char prev_char = 0;

    if (cur_node == UACPI_NULL)
        cur_node = uacpi_namespace_root();

    bytes_left = uacpi_strlen(path);

    for (;;) {
        if (bytes_left == 0)
            return cur_node;

        switch (*cursor) {
        case '\\':
            if (prev_char == '^')
                goto out_invalid_path;

            cur_node = uacpi_namespace_root();
            break;
        case '^':
            // Tried to go behind root
            if (uacpi_unlikely(cur_node == uacpi_namespace_root()))
                goto out_invalid_path;

            cur_node = cur_node->parent;
            break;
        default:
            break;
        }

        prev_char = *cursor;

        switch (prev_char) {
        case '^':
        case '\\':
            cursor++;
            bytes_left--;
            break;
        default:
            break;
        }

        if (prev_char != '^')
            break;
    }

    while (bytes_left != 0) {
        uacpi_object_name nameseg;

        if (*cursor == '.') {
            cursor++;
            bytes_left--;
        }

        if (uacpi_unlikely(bytes_left < 4))
            goto out_invalid_path;

        uacpi_memcpy(nameseg.text, cursor, sizeof(nameseg));
        cursor += sizeof(nameseg);
        bytes_left -= sizeof(nameseg);

        cur_node = uacpi_namespace_node_find_sub_node(cur_node, nameseg);
        if (cur_node == UACPI_NULL)
            return cur_node;
    }

    return cur_node;

out_invalid_path:
    uacpi_kernel_log(
        UACPI_LOG_WARN, "Invalid path '%s'\n",
        path
    );
    return UACPI_NULL;
}

uacpi_object *uacpi_namespace_node_get_object(uacpi_namespace_node *node)
{
    return uacpi_unwrap_internal_reference(node->object);
}

void uacpi_namespace_for_each_node_depth_first(
    uacpi_namespace_node *node,
    uacpi_iteration_callback callback,
    void *user
)
{
    uacpi_bool walking_up = UACPI_FALSE;
    uacpi_u32 depth = 1;

    if (node == UACPI_NULL)
        return;

    while (depth) {
        if (walking_up) {
            if (node->next) {
                node = node->next;
                walking_up = UACPI_FALSE;
                continue;
            }

            depth--;
            node = node->parent;
            continue;
        }

        switch (callback(user, node)) {
        case UACPI_NS_ITERATION_DECISION_CONTINUE:
            if (node->child) {
                node = node->child;
                depth++;
                continue;
            }
            // FALLTHROUGH intended
        case UACPI_NS_ITERATION_DECISION_NEXT_PEER:
            walking_up = UACPI_TRUE;
            continue;

        case UACPI_NS_ITERATION_DECISION_BREAK:
        default:
            return;
        }
    }
}

static uacpi_size node_depth(uacpi_namespace_node *node)
{
    uacpi_size depth = 0;

    while (node) {
        depth++;
        node = node->parent;
    }

    return depth;
}

const uacpi_char *uacpi_namespace_node_generate_absolute_path(
    uacpi_namespace_node *node
)
{
    uacpi_size depth, offset;
    uacpi_size bytes_needed;
    uacpi_char *path;

    depth = node_depth(node);
    if (depth == 0)
        return UACPI_NULL;

    // \ only needs 1 byte, the rest is 4 bytes
    bytes_needed = 1 + (depth - 1) * sizeof(uacpi_object_name);

    // \ and the first NAME don't need a '.', every other segment does
    bytes_needed += depth > 2 ? depth - 2 : 0;

    // Null terminator
    bytes_needed += 1;

    path = uacpi_kernel_alloc(bytes_needed);
    if (uacpi_unlikely(path == UACPI_NULL))
        return path;

    path[0] = '\\';

    offset = bytes_needed - 1;
    path[offset] = '\0';

    while (node != uacpi_namespace_root()) {
        offset -= sizeof(uacpi_object_name);
        uacpi_memcpy(&path[offset], node->name.text, sizeof(uacpi_object_name));

        node = node->parent;
        if (node != uacpi_namespace_root())
            path[--offset] = '.';
    }

    return path;
}
