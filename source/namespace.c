#include <uacpi/namespace.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/internal/types.h>
#include <uacpi/internal/stdlib.h>
#include <uacpi/internal/interpreter.h>
#include <uacpi/internal/opregion.h>
#include <uacpi/internal/log.h>
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
    case UACPI_PREDEFINED_NAMESPACE_ROOT:
        obj = uacpi_create_object(UACPI_OBJECT_DEVICE);
        if (uacpi_unlikely(obj == UACPI_NULL))
            return obj;

        /*
         * Erase the type here so that code like ObjectType(\) returns
         * the spec-compliant result of 0. We still create it as device
         * so that it is able to store global address space & notify handlers.
         */
        obj->type = UACPI_OBJECT_UNINITIALIZED;
        break;

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
        uacpi_shareable_init(node);

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
        uacpi_warn("requested invalid predefined namespace %d\n", ns);
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

    uacpi_shareable_init(ret);
    ret->name = name;
    return ret;
}

static void free_namespace_node(uacpi_handle handle)
{
    uacpi_namespace_node *node = handle;

    if (node->object)
        uacpi_object_unref(node->object);

    uacpi_free(node, sizeof(*node));
}

void uacpi_namespace_node_unref(uacpi_namespace_node *node)
{
    uacpi_shareable_unref_and_delete_if_last(node, free_namespace_node);
}

uacpi_status uacpi_node_install(
    uacpi_namespace_node *parent,
    uacpi_namespace_node *node
)
{
    uacpi_namespace_node *prev;

    if (parent == UACPI_NULL)
        parent = uacpi_namespace_root();

    if (uacpi_unlikely(uacpi_namespace_node_is_dangling(node))) {
        uacpi_warn("attempting to install a dangling namespace node %.4s\n",
                   node->name.text);
        return UACPI_STATUS_NAMESPACE_NODE_DANGLING;
    }

    prev = parent->child;
    parent->child = node;
    node->parent = parent;
    node->next = prev;

    if (prev) {
        if (uacpi_unlikely(prev->prev != UACPI_NULL)) {
            uacpi_warn(
                "while installing node @%p: previous node @%p already has "
                " a valid prev link @%p\n", node, prev, prev->prev
            );
        }

        prev->prev = node;
    }

    return UACPI_STATUS_OK;
}

uacpi_bool uacpi_namespace_node_is_dangling(uacpi_namespace_node *node)
{
    return node->flags & UACPI_NAMESPACE_NODE_FLAG_DANGLING;
}

void uacpi_node_uninstall(uacpi_namespace_node *node)
{
    uacpi_object *object;

    if (uacpi_unlikely(uacpi_namespace_node_is_dangling(node))) {
        uacpi_warn("attempting to uninstall a dangling namespace node %.4s\n",
                   node->name.text);
        return;
    }

    /*
     * Even though namespace_node is reference-counted it still has an 'invalid'
     * state that is entered after it is uninstalled from the global namespace.
     *
     * Reference counting is only needed to combat dangling pointer issues
     * whereas bad AML might try to prolong a local object lifetime by
     * returning it from a method, or CopyObject it somewhere. In that case the
     * namespace node object itself is still alive, but no longer has a valid
     * object associated with it.
     *
     * Example:
     *     Method (BAD) {
     *         OperationRegion(REG, SystemMemory, 0xDEADBEEF, 4)
     *         Field (REG, AnyAcc, NoLock) {
     *             FILD, 8,
     *         }
     *
     *         Return (RefOf(FILD))
     *     }
     *
     *     // Local0 is now the sole owner of the 'FILD' object that under the
     *     // hood is still referencing the 'REG' operation region object from
     *     // the 'BAD' method.
     *     Local0 = DerefOf(BAD())
     *
     * This is done to prevent potential very deep recursion where an object
     * frees a namespace node that frees an attached object that frees a
     * namespace node as well as potential infinite cycles between a namespace
     * node and an object.
     */
    object = uacpi_namespace_node_get_object(node);
    if (object != UACPI_NULL) {
        if (object->type == UACPI_OBJECT_OPERATION_REGION)
            uacpi_opregion_uninstall_handler(node);

        uacpi_object_unref(node->object);
        node->object = UACPI_NULL;
    }

    if (node->parent && node->parent->child == node)
        node->parent->child = node->next;

    if (node->prev)
        node->prev->next = node->next;
    if (node->next)
        node->next->prev = node->prev;

    if (uacpi_unlikely(node->child)) {
        uacpi_warn(
            "trying to uninstall a node @%p with a valid child link @%p\n",
            node, node->child
        );
    }

    node->flags |= UACPI_NAMESPACE_NODE_FLAG_DANGLING;
    uacpi_namespace_node_unref(node);
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

static uacpi_object_name segment_to_name(
    const uacpi_char **string, uacpi_size *in_out_size
)
{
    uacpi_object_name out_name;
    const uacpi_char *cursor = *string;
    uacpi_size offset, bytes_left = *in_out_size;

    for (offset = 0; offset < 4; offset++) {
        if (bytes_left < 1 || *cursor == '.') {
            out_name.text[offset] = '_';
            continue;
        }

        out_name.text[offset] = *cursor++;
        bytes_left--;
    }

    *string = cursor;
    *in_out_size = bytes_left;
    return out_name;
}

enum may_search_above_parent {
    MAY_SEARCH_ABOVE_PARENT_NO,
    MAY_SEARCH_ABOVE_PARENT_YES,
};

static uacpi_namespace_node *uacpi_namespace_node_do_find(
    uacpi_namespace_node *parent, const uacpi_char *path,
    enum may_search_above_parent may_search_above_parent
)
{
    uacpi_namespace_node *cur_node = parent;
    const uacpi_char *cursor = path;
    uacpi_size bytes_left;
    uacpi_char prev_char = 0;
    uacpi_bool single_nameseg = UACPI_TRUE;

    if (cur_node == UACPI_NULL)
        cur_node = uacpi_namespace_root();

    bytes_left = uacpi_strlen(path);

    for (;;) {
        if (bytes_left == 0)
            return cur_node;

        switch (*cursor) {
        case '\\':
            single_nameseg = UACPI_FALSE;

            if (prev_char == '^')
                goto out_invalid_path;

            cur_node = uacpi_namespace_root();
            break;
        case '^':
            single_nameseg = UACPI_FALSE;

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

        nameseg = segment_to_name(&cursor, &bytes_left);
        if (bytes_left != 0 && single_nameseg)
            single_nameseg = UACPI_FALSE;

        cur_node = uacpi_namespace_node_find_sub_node(cur_node, nameseg);
        if (cur_node == UACPI_NULL) {
            if (may_search_above_parent == MAY_SEARCH_ABOVE_PARENT_NO ||
                !single_nameseg)
                return cur_node;

            parent = parent->parent;

            while (parent) {
                cur_node = uacpi_namespace_node_find_sub_node(parent, nameseg);
                if (cur_node != UACPI_NULL)
                    return cur_node;

                parent = parent->parent;
            }

            return cur_node;
        }
    }

    return cur_node;

out_invalid_path:
    uacpi_warn("invalid path '%s'\n", path);
    return UACPI_NULL;
}

uacpi_namespace_node *uacpi_namespace_node_find(
    uacpi_namespace_node *parent, const uacpi_char *path
)
{
    return uacpi_namespace_node_do_find(
        parent, path, MAY_SEARCH_ABOVE_PARENT_NO
    );
}

uacpi_namespace_node *uacpi_namespace_node_resolve_from_aml_namepath(
    uacpi_namespace_node *scope, const uacpi_char *path
)
{
    return uacpi_namespace_node_do_find(
        scope, path, MAY_SEARCH_ABOVE_PARENT_YES
    );
}

uacpi_object *uacpi_namespace_node_get_object(uacpi_namespace_node *node)
{
    if (node == UACPI_NULL || node->object == UACPI_NULL)
        return UACPI_NULL;

    return uacpi_unwrap_internal_reference(node->object);
}

uacpi_object_name uacpi_namespace_node_name(uacpi_namespace_node *node)
{
    return node->name;
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
            UACPI_FALLTHROUGH;
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
