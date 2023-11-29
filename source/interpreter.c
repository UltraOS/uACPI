#include <uacpi/internal/types.h>
#include <uacpi/internal/interpreter.h>
#include <uacpi/internal/dynamic_array.h>
#include <uacpi/internal/opcodes.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/platform/stdlib.h>
#include <uacpi/kernel_api.h>
#include <uacpi/internal/context.h>

enum reference_kind {
    /*
     * Stores to this reference type change the referenced object.
     * The reference is created with this kind when a RefOf result is stored
     * in an object. Detailed explanation below.
     */
    REFERENCE_KIND_REFOF = 0,

    /*
     * Reference to a local variable, stores go into the referenced object
     * _unless_ the referenced object is a REFERENCE_KIND_REFOF. In that case,
     * the reference is unwound one more level as if the expression was
     * Store(..., DerefOf(ArgX))
     */
    REFERENCE_KIND_LOCAL = 1,

    /*
     * Reference to an argument. Same semantics for stores as
     * REFERENCE_KIND_LOCAL.
     */
    REFERENCE_KIND_ARG = 2,

    /*
     * Reference to a named object. Same semantics as REFERENCE_KIND_LOCAL.
     */
    REFERENCE_KIND_NAMED = 3,
};

/*
 * TODO: Write a note here explaining how references are currently implemented
 *       and how some of the edge cases are handled.
 */

enum item_type {
    ITEM_NONE = 0,
    ITEM_NAMESPACE_NODE,
    ITEM_OBJECT,
    ITEM_EMPTY_OBJECT,
    ITEM_PACKAGE_LENGTH,
    ITEM_IMMEDIATE,
};

struct package_length {
    uacpi_u32 begin;
    uacpi_u32 end;
};

struct item {
    uacpi_u8 type;
    union {
        uacpi_object *obj;
        struct uacpi_namespace_node *node;
        struct package_length pkg;
        uacpi_u64 immediate;
        uacpi_u8 immediate_bytes[8];
    };
};

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(item_array, struct item, 8)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(item_array, struct item, static)

struct op_context {
    uacpi_u8 pc;
    uacpi_bool preempted;

    /*
     * == 0 -> none
     * >= 1 -> item[idx - 1]
     */
    uacpi_u8 tracked_pkg_idx;

    const struct uacpi_op_spec *op;
    struct item_array items;
};

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(op_context_array, struct op_context, 8)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(
    op_context_array, struct op_context, static
)

static struct op_context *op_context_array_one_before_last(
    struct op_context_array *arr
)
{
    uacpi_size size;

    size = op_context_array_size(arr);

    if (size < 2)
        return UACPI_NULL;

    return op_context_array_at(arr, size - 2);
}

enum code_block_type {
    CODE_BLOCK_IF = 1,
    CODE_BLOCK_ELSE = 2,
    CODE_BLOCK_WHILE = 3,
    CODE_BLOCK_SCOPE = 4,
};

struct code_block {
    enum code_block_type type;
    uacpi_u32 begin, end;
    struct uacpi_namespace_node *node;
};

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(code_block_array, struct code_block, 8)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(
    code_block_array, struct code_block, static
)

struct call_frame {
    struct uacpi_control_method *method;

    uacpi_object *args[7];
    uacpi_object *locals[8];

    struct op_context_array pending_ops;
    struct code_block_array code_blocks;
    struct code_block *last_while;
    struct uacpi_namespace_node *cur_scope;

    uacpi_u32 code_offset;
};

static void *call_frame_cursor(struct call_frame *frame)
{
    return frame->method->code + frame->code_offset;
}

static uacpi_size call_frame_code_bytes_left(struct call_frame *frame)
{
    return frame->method->size - frame->code_offset;
}

static bool call_frame_has_code(struct call_frame* frame)
{
    return call_frame_code_bytes_left(frame) > 0;
}

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(call_frame_array, struct call_frame, 4)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(
    call_frame_array, struct call_frame, static
)

// NOTE: Try to keep size under 2 pages
struct execution_context {
    uacpi_object *ret;
    struct call_frame_array call_stack;

    struct call_frame *cur_frame;
    struct code_block *cur_block;
    struct uacpi_control_method *cur_method;
    const struct uacpi_op_spec *cur_op;
    struct op_context *prev_op_ctx;
    struct op_context *cur_op_ctx;

    uacpi_bool skip_else;
};

#define AML_READ(ptr, offset) (*(((uacpi_u8*)(code)) + offset))

/*
 * LeadNameChar := ‘A’-‘Z’ | ‘_’
 * DigitChar := ‘0’ - ‘9’
 * NameChar := DigitChar | LeadNameChar
 * ‘A’-‘Z’ := 0x41 - 0x5A
 * ‘_’ := 0x5F
 * ‘0’-‘9’ := 0x30 - 0x39
 */
static uacpi_status parse_nameseg(uacpi_u8 *cursor,
                                  uacpi_object_name *out_name)
{
    uacpi_size i;

    for (i = 0; i < 4; ++i) {
        uacpi_char data = cursor[i];

        if (data == '_')
            continue;
        if (data >= '0' && data <= '9')
            continue;
        if (data >= 'A' && data <= 'Z')
            continue;

        return UACPI_STATUS_BAD_BYTECODE;
    }

    uacpi_memcpy(&out_name->id, cursor, 4);
    return UACPI_STATUS_OK;
}

/*
 * -------------------------------------------------------------
 * RootChar := ‘\’
 * ParentPrefixChar := ‘^’
 * ‘\’ := 0x5C
 * ‘^’ := 0x5E
 * ------------------------------------------------------------
 * NameSeg := <leadnamechar namechar namechar namechar>
 * NameString := <rootchar namepath> | <prefixpath namepath>
 * PrefixPath := Nothing | <’^’ prefixpath>
 * NamePath := NameSeg | DualNamePath | MultiNamePath | NullName
 * DualNamePath := DualNamePrefix NameSeg NameSeg
 * MultiNamePath := MultiNamePrefix SegCount NameSeg(SegCount)
 */

enum resolve_behavior {
    RESOLVE_CREATE_LAST_NAMESEG_FAIL_IF_EXISTS,
    RESOLVE_FAIL_IF_DOESNT_EXIST,
};

static uacpi_status resolve_name_string(
    struct call_frame *frame,
    enum resolve_behavior behavior,
    struct uacpi_namespace_node **out_node
)
{
    uacpi_status ret;
    uacpi_u8 *cursor;
    uacpi_size bytes_left, namesegs;
    struct uacpi_namespace_node *parent, *cur_node = frame->cur_scope;
    uacpi_char prev_char = 0;
    uacpi_bool just_one_nameseg = UACPI_TRUE;

    bytes_left = call_frame_code_bytes_left(frame);
    cursor = call_frame_cursor(frame);

    for (;;) {
        if (uacpi_unlikely(bytes_left == 0))
            return UACPI_STATUS_BAD_BYTECODE;

        switch (*cursor) {
        case '\\':
            if (prev_char == '^')
                return UACPI_STATUS_BAD_BYTECODE;

            cur_node = UACPI_NULL;
            break;
        case '^':
            // Tried to go behind root
            if (uacpi_unlikely(cur_node == UACPI_NULL))
                return UACPI_STATUS_BAD_BYTECODE;

            cur_node = cur_node->parent;
            break;
        default:
            break;
        }

        prev_char = *cursor;

        switch (prev_char) {
        case '^':
        case '\\':
            just_one_nameseg = UACPI_FALSE;
            cursor++;
            bytes_left--;
            break;
        default:
            break;
        }

        if (prev_char != '^')
            break;
    }

    // At least a NullName byte is expected here
    if (uacpi_unlikely(bytes_left == 0))
        return UACPI_STATUS_BAD_BYTECODE;

    bytes_left--;
    switch (*cursor++)
    {
    case UACPI_DUAL_NAME_PREFIX:
        namesegs = 2;
        just_one_nameseg = UACPI_FALSE;
        break;
    case UACPI_MULTI_NAME_PREFIX:
        if (uacpi_unlikely(bytes_left == 0))
            return UACPI_STATUS_BAD_BYTECODE;
        namesegs = *cursor;
        cursor++;
        bytes_left--;
        just_one_nameseg = UACPI_FALSE;
        break;
    case UACPI_NULL_NAME:
        if (behavior == RESOLVE_CREATE_LAST_NAMESEG_FAIL_IF_EXISTS ||
            just_one_nameseg)
            return UACPI_STATUS_BAD_BYTECODE;

        goto out;
    default:
        /*
         * Might be an invalid byte, but assume single nameseg for now,
         * the code below will validate it for us.
         */
        cursor--;
        bytes_left++;
        namesegs = 1;
        break;
    }

    if (uacpi_unlikely((namesegs * 4) > bytes_left))
        return UACPI_STATUS_BAD_BYTECODE;

    for (; namesegs; cursor += 4, namesegs--) {
        uacpi_object_name name;

        ret = parse_nameseg(cursor, &name);
        if (uacpi_unlikely_error(ret))
            return ret;

        parent = cur_node;
        cur_node = uacpi_namespace_node_find(parent, name);

        switch (behavior) {
        case RESOLVE_CREATE_LAST_NAMESEG_FAIL_IF_EXISTS:
            if (namesegs == 1) {
                if (cur_node)
                    return UACPI_STATUS_BAD_BYTECODE;

                // Create the node and link to parent but don't install YET
                cur_node = uacpi_namespace_node_alloc(
                    name,
                    UACPI_OBJECT_UNINITIALIZED
                );
                cur_node->parent = parent;
            }
            break;
        case RESOLVE_FAIL_IF_DOESNT_EXIST:
            if (just_one_nameseg) {
                while (!cur_node && parent) {
                    cur_node = parent;
                    parent = cur_node->parent;

                    cur_node = uacpi_namespace_node_find(parent, name);
                }
            }
            break;
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
        }

        if (cur_node == UACPI_NULL)
            return UACPI_STATUS_NOT_FOUND;
    }

out:
    frame->code_offset = cursor - frame->method->code;
    *out_node = cur_node;
    return UACPI_STATUS_OK;
}

uacpi_status get_op(struct execution_context *ctx)
{
    uacpi_aml_op op;
    struct call_frame *frame = ctx->cur_frame;
    void *code = frame->method->code;
    uacpi_size size = frame->method->size;

    if (uacpi_unlikely(frame->code_offset >= size))
        return UACPI_STATUS_OUT_OF_BOUNDS;

    op = AML_READ(code, frame->code_offset++);
    if (op == UACPI_EXT_PREFIX) {
        if (uacpi_unlikely(frame->code_offset >= size))
            return UACPI_STATUS_OUT_OF_BOUNDS;

        op <<= 8;
        op |= AML_READ(code, frame->code_offset++);
    }

    ctx->cur_op = uacpi_get_op_spec(op);
    if (uacpi_unlikely(ctx->cur_op->properties & UACPI_OP_PROPERTY_RESERVED))
        return UACPI_STATUS_BAD_BYTECODE;

    return UACPI_STATUS_OK;
}

uacpi_status handle_string(struct execution_context *ctx)
{
    struct call_frame *frame = ctx->cur_frame;
    uacpi_object *obj;

    char *string;
    size_t length;

    obj = item_array_last(&ctx->cur_op_ctx->items)->obj;
    string = call_frame_cursor(frame);

    // TODO: sanitize string for valid UTF-8
    length = uacpi_strnlen(string, call_frame_code_bytes_left(frame));

    if (string[length++] != 0x00)
        return UACPI_STATUS_BAD_BYTECODE;

    obj->buffer->text = uacpi_kernel_alloc(length);
    if (uacpi_unlikely(obj->buffer->text == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    uacpi_memcpy(obj->buffer->text, string, length);
    obj->buffer->size = length;
    frame->code_offset += length;
    return UACPI_STATUS_OK;
}

struct object_storage_as_buffer {
    void *ptr;
    uacpi_size len;
};

static uacpi_status get_object_storage(uacpi_object *obj,
                                       struct object_storage_as_buffer *out_buf)
{
    switch (obj->type) {
    case UACPI_OBJECT_INTEGER:
        out_buf->len = g_uacpi_rt_ctx.is_rev1 ? 4 : 8;
        out_buf->ptr = &obj->integer;
        break;
    case UACPI_OBJECT_STRING:
        out_buf->len = obj->buffer->size ? obj->buffer->size - 1 : 0;
        out_buf->ptr = obj->buffer->text;
        break;
    case UACPI_OBJECT_BUFFER:
        if (obj->buffer->size == 0) {
            out_buf->len = 0;
            break;
        }

        out_buf->len = obj->buffer->size - (obj->type == UACPI_OBJECT_STRING);
        out_buf->ptr = obj->buffer->data;
        break;
    case UACPI_OBJECT_REFERENCE:
        return UACPI_STATUS_INVALID_ARGUMENT;
    default:
        return UACPI_STATUS_BAD_BYTECODE;
    }

    return UACPI_STATUS_OK;
}

/*
 * The word "implicit cast" here is only because it's called that in
 * the specification. In reality, we just copy one buffer to another
 * because that's what NT does.
 */
static uacpi_status object_assign_with_implicit_cast(uacpi_object *dst,
                                                     uacpi_object *src)
{
    uacpi_status ret;
    struct object_storage_as_buffer src_buf, dst_buf;
    uacpi_size bytes_to_copy;

    ret = get_object_storage(src, &src_buf);
    if (uacpi_unlikely_error(ret))
        return ret;

    ret = get_object_storage(dst, &dst_buf);
    if (uacpi_unlikely_error(ret))
        return ret;

    bytes_to_copy = UACPI_MIN(src_buf.len, dst_buf.len);
    uacpi_memcpy(dst_buf.ptr, src_buf.ptr, bytes_to_copy);
    uacpi_memzero((uacpi_u8*)dst_buf.ptr + bytes_to_copy,
                  dst_buf.len - bytes_to_copy);

    return ret;
}

static uacpi_object *object_deref_if_internal(uacpi_object *object)
{
    for (;;) {
        if (object->type != UACPI_OBJECT_REFERENCE ||
            object->flags == REFERENCE_KIND_REFOF)
            return object;

        object = object->inner_object;
    }
}

enum argx_or_localx {
    ARGX,
    LOCALX,
};

static uacpi_status handle_arg_or_local(
    struct execution_context *ctx,
    uacpi_size idx, enum argx_or_localx type
)
{
    uacpi_object **src, *dst;
    enum reference_kind kind;

    dst = item_array_last(&ctx->cur_op_ctx->items)->obj;

    if (type == ARGX) {
        src = &ctx->cur_frame->args[idx];
        kind = REFERENCE_KIND_ARG;
    } else {
        src = &ctx->cur_frame->locals[idx];
        kind = REFERENCE_KIND_LOCAL;
    }

    // Access to an uninitialized local or arg, hopefully a store incoming
    if (*src == UACPI_NULL) {
        *src = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);
        if (uacpi_unlikely(*src == UACPI_NULL))
            return UACPI_STATUS_OUT_OF_MEMORY;
    }

    dst->flags = kind;
    dst->type = UACPI_OBJECT_REFERENCE;
    dst->inner_object = *src;
    uacpi_object_ref(*src);

    return UACPI_STATUS_OK;
}

static uacpi_status handle_local(struct execution_context *ctx)
{
    uacpi_size idx;
    struct op_context *op_ctx = ctx->cur_op_ctx;

    idx = op_ctx->op->code - UACPI_AML_OP_Local0Op;
    return handle_arg_or_local(ctx, idx, LOCALX);
}

static uacpi_status handle_arg(struct execution_context *ctx)
{
    uacpi_size idx;
    struct op_context *op_ctx = ctx->cur_op_ctx;

    idx = op_ctx->op->code - UACPI_AML_OP_Arg0Op;
    return handle_arg_or_local(ctx, idx, ARGX);
}

static uacpi_status handle_named_object(struct execution_context *ctx)
{
    struct uacpi_namespace_node *src;
    uacpi_object *dst;

    src = item_array_at(&ctx->cur_op_ctx->items, 0)->node;
    dst = item_array_at(&ctx->cur_op_ctx->items, 1)->obj;

    dst->type = UACPI_OBJECT_REFERENCE;
    dst->flags = REFERENCE_KIND_NAMED;
    dst->inner_object = src->object;
    uacpi_object_ref(src->object);

    return UACPI_STATUS_OK;
}

static void truncate_number_if_needed(uacpi_object *obj)
{
    if (!g_uacpi_rt_ctx.is_rev1)
        return;

    obj->integer &= 0xFFFFFFFF;
}

static uacpi_u64 ones()
{
    return g_uacpi_rt_ctx.is_rev1 ? 0xFFFFFFFF : 0xFFFFFFFFFFFFFFFF;
}

static uacpi_status method_get_ret_target(struct execution_context *ctx,
                                          uacpi_object **out_operand)
{
    uacpi_size depth;

    // Check if we're targeting the previous call frame
    depth = call_frame_array_size(&ctx->call_stack);
    if (depth > 1) {
        struct op_context *op_ctx;
        struct call_frame *frame;

        frame = call_frame_array_at(&ctx->call_stack, depth - 2);
        depth = op_context_array_size(&frame->pending_ops);

        // Ok, no one wants the return value at call site. Discard it.
        if (!depth) {
            *out_operand = UACPI_NULL;
            return UACPI_STATUS_OK;
        }

        op_ctx = op_context_array_at(&frame->pending_ops, depth - 1);
        *out_operand = item_array_last(&op_ctx->items)->obj;
        return UACPI_STATUS_OK;
    }

    return UACPI_STATUS_NOT_FOUND;
}

static uacpi_status method_get_ret_object(struct execution_context *ctx,
                                          uacpi_object **out_obj)
{
    uacpi_status ret;

    ret = method_get_ret_target(ctx, out_obj);
    if (ret == UACPI_STATUS_NOT_FOUND) {
        *out_obj = ctx->ret;
        return UACPI_STATUS_OK;
    }
    if (ret != UACPI_STATUS_OK || *out_obj == UACPI_NULL)
        return ret;

    *out_obj = object_deref_if_internal(*out_obj);
    return UACPI_STATUS_OK;
}

static struct code_block *find_last_block(struct code_block_array *blocks,
                                          enum code_block_type type)
{
    uacpi_size i;

    i = code_block_array_size(blocks);
    while (i-- > 0) {
        struct code_block *block;

        block = code_block_array_at(blocks, i);
        if (block->type == type)
            return block;
    }

    return UACPI_NULL;
}

static void update_scope(struct call_frame *frame)
{
    struct code_block *block;

    block = find_last_block(&frame->code_blocks, CODE_BLOCK_SCOPE);
    if (block == UACPI_NULL) {
        frame->cur_scope = UACPI_NULL;
        return;
    }

    frame->cur_scope = block->node;
}

static uacpi_status begin_block_execution(struct execution_context *ctx)
{
    struct call_frame *cur_frame = ctx->cur_frame;
    struct op_context *op_ctx = ctx->cur_op_ctx;
    struct package_length *pkg;
    struct code_block *block;

    block = code_block_array_alloc(&cur_frame->code_blocks);
    if (uacpi_unlikely(block == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    switch (op_ctx->op->code) {
    case UACPI_AML_OP_IfOp:
        block->type = CODE_BLOCK_IF;
        break;
    case UACPI_AML_OP_ElseOp:
        block->type = CODE_BLOCK_ELSE;
        break;
    case UACPI_AML_OP_WhileOp:
        block->type = CODE_BLOCK_WHILE;
        break;
    case UACPI_AML_OP_ScopeOp:
        block->type = CODE_BLOCK_SCOPE;
        block->node = item_array_at(&op_ctx->items, 1)->node;
        break;
    default:
        code_block_array_pop(&cur_frame->code_blocks);
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    pkg = &item_array_at(&op_ctx->items, 0)->pkg;

    // -1 because we want to re-evaluate at the start of the op next time
    block->begin = pkg->begin - 1;
    block->end = pkg->end;
    ctx->cur_block = block;

    cur_frame->last_while = find_last_block(&cur_frame->code_blocks,
                                            CODE_BLOCK_WHILE);
    update_scope(cur_frame);
    return UACPI_STATUS_OK;
}

static void frame_reset_post_end_block(struct execution_context *ctx,
                                       enum code_block_type type)
{
    struct call_frame *frame = ctx->cur_frame;
    code_block_array_pop(&frame->code_blocks);
    ctx->cur_block = code_block_array_last(&frame->code_blocks);

    if (type == CODE_BLOCK_WHILE) {
        frame->last_while = find_last_block(&frame->code_blocks, type);
    } else if (type == CODE_BLOCK_SCOPE) {
        update_scope(frame);
    }
}

static uacpi_status debug_store(uacpi_object *dst, uacpi_object *src)
{
    src = object_deref_if_internal(src);

    switch (src->type) {
    case UACPI_OBJECT_UNINITIALIZED:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Uninitialized]\n");
        break;
    case UACPI_OBJECT_STRING:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, String] %s\n",
                         src->buffer->text);
        break;
    case UACPI_OBJECT_INTEGER:
        if (g_uacpi_rt_ctx.is_rev1) {
            uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Integer] 0x%08llX\n",
                             src->integer);
        } else {
            uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Integer] 0x%016llX\n",
                             src->integer);
        }
        break;
    case UACPI_OBJECT_REFERENCE:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Reference] Object @%p\n",
                         src);
        break;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }

    return UACPI_STATUS_OK;
}

uacpi_object *reference_unwind(uacpi_object *obj)
{
    while (obj) {
        if (obj->type != UACPI_OBJECT_REFERENCE)
            return obj;

        obj = obj->inner_object;
    }

    // This should be unreachable
    return UACPI_NULL;
}

/*
 * Object implicit dereferencing [Store(..., obj)/Increment(obj),...] behavior:
 * RefOf -> the bottom-most referenced object
 * LocalX/ArgX -> object stored at LocalX if LocalX is not a reference,
 *                otherwise goto RefOf case.
 * NAME -> object stored at NAME
 */
static uacpi_object *object_deref_implicit(uacpi_object *obj)
{
    if (obj->flags != REFERENCE_KIND_REFOF) {
        if (obj->flags == REFERENCE_KIND_NAMED ||
            obj->inner_object->type != UACPI_OBJECT_REFERENCE)
            return obj->inner_object;

        obj = obj->inner_object;
    }

    return reference_unwind(obj);
}

/*
 * Breakdown of what happens here:
 *
 * CopyObject(..., Obj) where Obj is:
 * 1. LocalX -> Overwrite LocalX.
 * 2. NAME -> Overwrite NAME.
 * 3. ArgX -> Overwrite ArgX unless ArgX is a reference, in that case
 *            overwrite the referenced object.
 * 4. RefOf -> Not allowed here.
 */
 static uacpi_status copy_object_to_reference(uacpi_object *dst,
                                              uacpi_object *src)
{
    uacpi_object *dst_obj;
    uacpi_object *src_obj;

    switch (dst->flags) {
    case REFERENCE_KIND_ARG: {
        uacpi_object *referenced_obj;

        referenced_obj = object_deref_if_internal(dst);
        if (referenced_obj->type == UACPI_OBJECT_REFERENCE &&
            referenced_obj->flags == REFERENCE_KIND_REFOF) {
            dst_obj = reference_unwind(referenced_obj);
            break;
        }

        // FALLTHROUGH intended here
    }
    case REFERENCE_KIND_LOCAL:
    case REFERENCE_KIND_NAMED:
        dst_obj = dst->inner_object;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    src_obj = object_deref_if_internal(src);
    return uacpi_object_assign(dst_obj, src_obj,
                               UACPI_ASSIGN_BEHAVIOR_DEEP_COPY);
}

/*
 * if Store(..., Obj) where Obj is:
 * 1. LocalX -> OVERWRITE unless the object is a reference, in that
 *              case store to the referenced object _with_ implicit
 *              cast.
 * 2. ArgX -> OVERWRITE unless the object is a reference, in that
 *            case OVERWRITE the referenced object.
 * 3. NAME -> Store with implicit cast.
 * 4. RefOf -> Not allowed here.
 */
static uacpi_status store_to_reference(uacpi_object *dst,
                                       uacpi_object *src)
{
    uacpi_object *dst_obj;
    uacpi_object *src_obj;
    uacpi_bool overwrite = UACPI_FALSE;

    switch (dst->flags) {
    case REFERENCE_KIND_LOCAL:
    case REFERENCE_KIND_ARG: {
        uacpi_object *referenced_obj;

        referenced_obj = object_deref_if_internal(dst);
        if (referenced_obj->type == UACPI_OBJECT_REFERENCE &&
            referenced_obj->flags == REFERENCE_KIND_REFOF) {
            dst_obj = reference_unwind(referenced_obj);
            overwrite = dst->flags == REFERENCE_KIND_ARG;
            break;
        }

        overwrite = UACPI_TRUE;
        dst_obj = dst->inner_object;
        break;
    }
    case REFERENCE_KIND_NAMED:
        dst_obj = reference_unwind(dst);
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    src_obj = object_deref_if_internal(src);
    overwrite |= dst_obj->type == UACPI_OBJECT_UNINITIALIZED;

    if (overwrite) {
        return uacpi_object_assign(dst_obj, src_obj,
                                   UACPI_ASSIGN_BEHAVIOR_DEEP_COPY);
    }

    return object_assign_with_implicit_cast(dst_obj, src_obj);
}

static uacpi_status handle_inc_dec(struct execution_context *ctx)
{
    uacpi_object *obj;
    struct op_context *op_ctx = ctx->cur_op_ctx;

    obj = item_array_at(&op_ctx->items, 0)->obj;

    if (op_ctx->op->code == UACPI_AML_OP_IncrementOp)
        obj->integer++;
    else
        obj->integer--;

    return UACPI_STATUS_OK;
}

static uacpi_status handle_ref_or_deref_of(struct execution_context *ctx)
{
    struct op_context *op_ctx = ctx->cur_op_ctx;
    uacpi_object *dst, *src;

    src = item_array_at(&op_ctx->items, 0)->obj;
    dst = item_array_at(&op_ctx->items, 1)->obj;

    if (op_ctx->op->code == UACPI_AML_OP_DerefOfOp) {
        /*
         * Explicit dereferencing [DerefOf] behavior:
         * Simply grabs the bottom-most object that is not a reference.
         * This mimics the behavior of NT Acpi.sys: any DerfOf fetches
         * the bottom-most reference. Note that this is different from
         * ACPICA where DerefOf dereferences one level.
         */
        src = reference_unwind(src);
        return uacpi_object_assign(dst, src,
                                   UACPI_ASSIGN_BEHAVIOR_SHALLOW_COPY);
    }

    dst->type = UACPI_OBJECT_REFERENCE;
    dst->inner_object = src;
    uacpi_object_ref(src);
    return UACPI_STATUS_OK;
}

static void do_binary_math(uacpi_object *arg0, uacpi_object *arg1,
                           uacpi_object *tgt0, uacpi_object *tgt1,
                           uacpi_aml_op op)
{
    uacpi_u64 lhs, rhs, res;
    uacpi_bool should_negate = UACPI_FALSE;

    lhs = arg0->integer;
    rhs = arg1->integer;

    switch (op)
    {
    case UACPI_AML_OP_AddOp:
        res = lhs + rhs;
        break;
    case UACPI_AML_OP_SubtractOp:
        res = lhs - rhs;
        break;
    case UACPI_AML_OP_MultiplyOp:
        res = lhs * rhs;
        break;
    case UACPI_AML_OP_ShiftLeftOp:
    case UACPI_AML_OP_ShiftRightOp:
        if (rhs <= (g_uacpi_rt_ctx.is_rev1 ? 31 : 63)) {
            if (op == UACPI_AML_OP_ShiftLeftOp)
                res = lhs << rhs;
            else
                res = lhs >> rhs;
        } else {
            res = 0;
        }
        break;
    case UACPI_AML_OP_NandOp:
        should_negate = UACPI_TRUE;
    case UACPI_AML_OP_AndOp:
        res = rhs & lhs;
        break;
    case UACPI_AML_OP_NorOp:
        should_negate = UACPI_TRUE;
    case UACPI_AML_OP_OrOp:
        res = rhs | lhs;
        break;
    case UACPI_AML_OP_XorOp:
        res = rhs ^ lhs;
        break;
    case UACPI_AML_OP_DivideOp:
        if (uacpi_likely(rhs > 0)) {
            tgt1->integer = lhs / rhs;
        } else {
            uacpi_kernel_log(UACPI_LOG_WARN, "Attempted division by zero!\n");
            tgt1->integer = 0;
        }
        // FALLTHROUGH intended here
    case UACPI_AML_OP_ModOp:
        res = lhs % rhs;
        break;
    default:
        break;
    }

    if (should_negate)
        res = ~res;

    tgt0->integer = res;
}

static uacpi_status handle_binary_math(struct execution_context *ctx)
{
    uacpi_object *arg0, *arg1, *tgt0, *tgt1;
    struct item_array *items = &ctx->cur_op_ctx->items;
    uacpi_aml_op op = ctx->cur_op_ctx->op->code;

    arg0 = item_array_at(items, 0)->obj;
    arg1 = item_array_at(items, 1)->obj;

    if (op == UACPI_AML_OP_DivideOp) {
        tgt0 = item_array_at(items, 4)->obj;
        tgt1 = item_array_at(items, 5)->obj;
    } else {
        tgt0 = item_array_at(items, 3)->obj;
        tgt1 = UACPI_NULL;
    }

    do_binary_math(arg0, arg1, tgt0, tgt1, op);
    return UACPI_STATUS_OK;
}

static uacpi_status handle_logical_not(struct execution_context *ctx)
{
    struct op_context *op_ctx = ctx->cur_op_ctx;
    uacpi_object *src, *dst;

    src = item_array_at(&op_ctx->items, 0)->obj;
    dst = item_array_at(&op_ctx->items, 1)->obj;

    dst->type = UACPI_OBJECT_INTEGER;
    dst->integer = src->integer ? 0 : ones();

    return UACPI_STATUS_OK;
}

static uacpi_status handle_logical_equality(struct execution_context *ctx)
{
    struct op_context *op_ctx = ctx->cur_op_ctx;
    uacpi_object *lhs, *rhs, *dst;
    uacpi_bool res;

    lhs = item_array_at(&op_ctx->items, 0)->obj;
    rhs = item_array_at(&op_ctx->items, 1)->obj;
    dst = item_array_at(&op_ctx->items, 2)->obj;

    // TODO: typecheck at parse time
    if (lhs->type != rhs->type)
        return UACPI_STATUS_BAD_BYTECODE;

    switch (lhs->type) {
    case UACPI_OBJECT_INTEGER:
        res = lhs->integer == rhs->integer;
        break;
    case UACPI_OBJECT_STRING:
    case UACPI_OBJECT_BUFFER:
        res = lhs->buffer->size == rhs->buffer->size;
        if (res && lhs->buffer->size) {
            res = uacpi_memcmp(
                lhs->buffer->data,
                rhs->buffer->data,
                lhs->buffer->size
            ) == 0;
        }
        break;
    default:
        // TODO: Type check at parse time
        return UACPI_STATUS_BAD_BYTECODE;
    }

    dst->type = UACPI_OBJECT_INTEGER;
    dst->integer = res ? ones() : 0;
    return UACPI_STATUS_OK;
}

/*
 * PkgLength :=
 *   PkgLeadByte |
 *   <pkgleadbyte bytedata> |
 *   <pkgleadbyte bytedata bytedata> | <pkgleadbyte bytedata bytedata bytedata>
 * PkgLeadByte :=
 *   <bit 7-6: bytedata count that follows (0-3)>
 *   <bit 5-4: only used if pkglength < 63>
 *   <bit 3-0: least significant package length nybble>
 */
static uacpi_status parse_package_length(struct call_frame *frame,
                                         struct package_length *out_pkg)
{
    uacpi_u32 left, size;
    uacpi_u8 *data, marker_length;

    out_pkg->begin = frame->code_offset;
    marker_length = 1;

    left = call_frame_code_bytes_left(frame);
    if (uacpi_unlikely(left < 1))
        return UACPI_STATUS_BAD_BYTECODE;

    data = call_frame_cursor(frame);
    marker_length += *data >> 6;

    if (uacpi_unlikely(left < marker_length))
        return UACPI_STATUS_BAD_BYTECODE;

    switch (marker_length) {
    case 1:
        size = *data & 0b111111;
        break;
    case 2:
    case 3:
    case 4: {
        uacpi_u32 temp_byte = 0;

        size = *data & 0b1111;
        uacpi_memcpy(&temp_byte, data + 1, marker_length - 1);

        // marker_length - 1 is at most 3, so this shift is safe
        size |= temp_byte << 4;
        break;
    }
    }

    frame->code_offset += marker_length;
    out_pkg->end = out_pkg->begin + size;
    return UACPI_STATUS_OK;
}

/*
 * ByteData
 * // bit 0-2: ArgCount (0-7)
 * // bit 3: SerializeFlag
 * //   0 NotSerialized
 * //   1 Serialized
 * // bit 4-7: SyncLevel (0x00-0x0f)
 */
void init_method_flags(uacpi_control_method *method, uacpi_u8 flags_byte)
{
    method->args = flags_byte & 0b111;
    method->is_serialized = (flags_byte >> 3) & 1;
    method->sync_level = flags_byte >> 4;
}

static uacpi_status handle_create_method(struct execution_context *ctx)
{
    struct op_context *op_ctx = ctx->cur_op_ctx;
    struct uacpi_control_method *method;
    struct package_length *pkg;
    struct uacpi_namespace_node *node;
    uacpi_u32 method_begin_offset;

    method = uacpi_kernel_alloc(sizeof(*method));
    if (uacpi_unlikely(method == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    pkg = &item_array_at(&op_ctx->items, 0)->pkg;
    node = item_array_at(&op_ctx->items, 1)->node;
    init_method_flags(method, item_array_at(&op_ctx->items, 2)->immediate);

    method_begin_offset = item_array_at(&op_ctx->items, 3)->immediate;
    method->code = ctx->cur_frame->method->code;
    method->code += method_begin_offset;
    method->size = pkg->end - method_begin_offset;

    node->object->type = UACPI_OBJECT_METHOD;
    node->object->method = method;
    method->node = node;

    return UACPI_STATUS_OK;
}

static uacpi_status handle_control_flow(struct execution_context *ctx)
{
    struct call_frame *frame = ctx->cur_frame;
    struct op_context *op_ctx = ctx->cur_op_ctx;

    for (;;) {
        if (ctx->cur_block != frame->last_while) {
            frame_reset_post_end_block(ctx, ctx->cur_block->type);
            continue;
        }

        if (op_ctx->op->code == UACPI_AML_OP_BreakOp)
            frame->code_offset = ctx->cur_block->end;
        else
            frame->code_offset = ctx->cur_block->begin;
        frame_reset_post_end_block(ctx, ctx->cur_block->type);
        break;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status handle_code_block(struct execution_context *ctx)
{
    struct op_context *op_ctx = ctx->cur_op_ctx;
    struct package_length *pkg;
    uacpi_bool skip_block;

    pkg = &item_array_at(&op_ctx->items, 0)->pkg;

    switch (op_ctx->op->code) {
    case UACPI_AML_OP_ElseOp:
        skip_block = ctx->skip_else;
        break;
    case UACPI_AML_OP_ScopeOp:
        skip_block = UACPI_FALSE;
        break;
    case UACPI_AML_OP_IfOp:
    case UACPI_AML_OP_WhileOp: {
        uacpi_object *operand;

        operand = item_array_at(&op_ctx->items, 1)->obj;
        skip_block = operand->integer == 0;
        break;
    }
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    if (skip_block) {
        ctx->cur_frame->code_offset = pkg->end;
        return UACPI_STATUS_OK;
    }

    return begin_block_execution(ctx);
}

static uacpi_status handle_return(struct execution_context *ctx)
{
    uacpi_status ret;
    uacpi_object *dst = UACPI_NULL;

    ctx->cur_frame->code_offset = ctx->cur_frame->method->size;
    ret = method_get_ret_object(ctx, &dst);

    if (uacpi_unlikely_error(ret))
        return ret;
    if (dst == UACPI_NULL)
        return UACPI_STATUS_OK;

    /*
     * Should be possible to move here if method returns a literal
     * like Return(Buffer { ... }), otherwise we have to copy just to
     * be safe.
     */
    return uacpi_object_assign(
        dst,
        item_array_at(&ctx->cur_op_ctx->items, 0)->obj,
        UACPI_ASSIGN_BEHAVIOR_DEEP_COPY
    );
}

static void execution_context_release(struct execution_context *ctx)
{
    if (ctx->ret)
        uacpi_object_unref(ctx->ret);
    call_frame_array_clear(&ctx->call_stack);
    uacpi_kernel_free(ctx);
}

static void call_frame_clear(struct call_frame *frame)
{
    uacpi_size i;

    op_context_array_clear(&frame->pending_ops);
    code_block_array_clear(&frame->code_blocks);

    for (i = 0; i < 7; ++i)
        uacpi_object_unref(frame->args[i]);
    for (i = 0; i < 8; ++i)
        uacpi_object_unref(frame->locals[i]);
}

static void refresh_ctx_pointers(struct execution_context *ctx)
{
    struct call_frame *frame = ctx->cur_frame;

    if (frame == UACPI_NULL) {
        ctx->cur_op_ctx = UACPI_NULL;
        ctx->prev_op_ctx = UACPI_NULL;
        ctx->cur_block = UACPI_NULL;
        return;
    }

    ctx->cur_op_ctx = op_context_array_last(&frame->pending_ops);
    ctx->prev_op_ctx = op_context_array_one_before_last(&frame->pending_ops);
    ctx->cur_block = code_block_array_last(&frame->code_blocks);
}

static void ctx_reload_post_ret(struct execution_context *ctx)
{
    call_frame_clear(ctx->cur_frame);
    call_frame_array_pop(&ctx->call_stack);

    ctx->cur_frame = call_frame_array_last(&ctx->call_stack);
    refresh_ctx_pointers(ctx);
}

static bool ctx_has_non_preempted_op(struct execution_context *ctx)
{
    return ctx->cur_op_ctx && !ctx->cur_op_ctx->preempted;
}

#define UACPI_OP_TRACING

static void trace_op(const struct uacpi_op_spec *op)
{
#ifdef UACPI_OP_TRACING
    uacpi_kernel_log(UACPI_LOG_TRACE, "Processing Op '%s' (0x%04X)\n",
                     op->name, op->code);
#endif
}

static void frame_push_args(struct call_frame *frame,
                            struct op_context *op_ctx)
{
    uacpi_size i;

    /*
     * MethodCall items:
     * items[0] -> method namespace node
     * items[1...nargs-1] -> method arguments
     * items[-1] -> return value object
     *
     * Here we only care about the arguments though.
     */
    for (i = 1; i < item_array_size(&op_ctx->items) - 1; i++) {
        frame->args[i - 1] = item_array_at(&op_ctx->items, i)->obj;
        uacpi_object_ref(frame->args[i - 1]);
    }
}

static uacpi_status frame_setup_base_scope(struct call_frame *frame,
                                           struct uacpi_control_method *method)
{
    struct code_block *block;

    block = code_block_array_alloc(&frame->code_blocks);
    if (uacpi_unlikely(block == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    block->type = CODE_BLOCK_SCOPE;
    block->node = method->node;
    block->begin = 0;
    block->end = method->size;
    frame->cur_scope = method->node;
    return UACPI_STATUS_OK;
}

static uacpi_status push_new_frame(struct execution_context *ctx,
                                   struct call_frame **out_frame)
{
    struct call_frame_array *call_stack = &ctx->call_stack;
    struct call_frame *prev_frame;

    *out_frame = call_frame_array_calloc(call_stack);
    if (uacpi_unlikely(*out_frame == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    /*
     * Allocating a new frame might have reallocated the dynamic buffer so our
     * execution_context members might now be pointing to freed memory.
     * Refresh them here.
     */
    prev_frame = call_frame_array_at(call_stack,
                                     call_frame_array_size(call_stack) - 2);
    ctx->cur_frame = prev_frame;
    refresh_ctx_pointers(ctx);

    return UACPI_STATUS_OK;
}

static uacpi_bool maybe_end_block(struct execution_context *ctx)
{
    struct code_block *block = ctx->cur_block;
    struct call_frame *cur_frame = ctx->cur_frame;
    uacpi_bool ret = UACPI_FALSE;

    if (!block)
        return ret;
    if (cur_frame->code_offset != block->end)
        return ret;

    ctx->skip_else = UACPI_FALSE;

    if (block->type == CODE_BLOCK_WHILE) {
        cur_frame->code_offset = block->begin;
    } else if (block->type == CODE_BLOCK_IF) {
        ctx->skip_else = UACPI_TRUE;
        ret = UACPI_TRUE;
    }

    frame_reset_post_end_block(ctx, block->type);
    return ret;
}

static uacpi_status store_to_target(uacpi_object *dst, uacpi_object *src)
{
    uacpi_status ret;

    switch (dst->type) {
    case UACPI_OBJECT_DEBUG:
        ret = debug_store(dst, src);
        break;
    case UACPI_OBJECT_REFERENCE:
        ret = store_to_reference(dst, src);
        break;
    case UACPI_OBJECT_INTEGER:
        // NULL target
        if (dst->integer == 0) {
            ret = UACPI_STATUS_OK;
            break;
        }
    default:
        ret = UACPI_STATUS_BAD_BYTECODE;
    }

    return ret;
}

static uacpi_status handle_copy_object_or_store(struct execution_context *ctx)
{
    uacpi_object *src, *dst;
    struct op_context *op_ctx = ctx->cur_op_ctx;

    src = item_array_at(&op_ctx->items, 0)->obj;
    dst = item_array_at(&op_ctx->items, 1)->obj;

    if (op_ctx->op->code == UACPI_AML_OP_StoreOp)
        return store_to_target(dst, src);

    return copy_object_to_reference(dst, src);
}

static uacpi_status push_op(struct execution_context *ctx)
{
    struct call_frame *frame = ctx->cur_frame;
    struct op_context *op_ctx;

    op_ctx = op_context_array_calloc(&frame->pending_ops);
    if (op_ctx == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    op_ctx->op = ctx->cur_op;
    refresh_ctx_pointers(ctx);
    return UACPI_STATUS_OK;
}

static void pop_op(struct execution_context *ctx)
{
    struct call_frame *frame = ctx->cur_frame;
    struct op_context *cur_op_ctx = ctx->cur_op_ctx;

    for (;;) {
        struct item *item;

        item = item_array_last(&cur_op_ctx->items);

        if (item == UACPI_NULL)
            break;
        if (item->type == ITEM_OBJECT)
            uacpi_object_unref(item->obj);

        item_array_pop(&cur_op_ctx->items);
    }

    item_array_clear(&cur_op_ctx->items);
    op_context_array_pop(&frame->pending_ops);
    refresh_ctx_pointers(ctx);
}

static uacpi_u8 parse_op_generates_item[0x100] = {
    [UACPI_PARSE_OP_SIMPLE_NAME] = ITEM_EMPTY_OBJECT,
    [UACPI_PARSE_OP_SUPERNAME] = ITEM_EMPTY_OBJECT,
    [UACPI_PARSE_OP_SUPERNAME_IMPLICIT_DEREF] = ITEM_EMPTY_OBJECT,
    [UACPI_PARSE_OP_TERM_ARG] = ITEM_EMPTY_OBJECT,
    [UACPI_PARSE_OP_TERM_ARG_UNWRAP_INTERNAL] = ITEM_EMPTY_OBJECT,
    [UACPI_PARSE_OP_OPERAND] = ITEM_EMPTY_OBJECT,
    [UACPI_PARSE_OP_TARGET] = ITEM_EMPTY_OBJECT,
    [UACPI_PARSE_OP_PKGLEN] = ITEM_PACKAGE_LENGTH,
    [UACPI_PARSE_OP_TRACKED_PKGLEN] = ITEM_PACKAGE_LENGTH,
    [UACPI_PARSE_OP_CREATE_NAMESTRING] = ITEM_NAMESPACE_NODE,
    [UACPI_PARSE_OP_EXISTING_NAMESTRING] = ITEM_NAMESPACE_NODE,
    [UACPI_PARSE_OP_LOAD_INLINE_IMM_AS_OBJECT] = ITEM_OBJECT,
    [UACPI_PARSE_OP_LOAD_IMM] = ITEM_IMMEDIATE,
    [UACPI_PARSE_OP_LOAD_IMM_AS_OBJECT] = ITEM_OBJECT,
    [UACPI_PARSE_OP_OBJECT_ALLOC] = ITEM_OBJECT,
    [UACPI_PARSE_OP_OBJECT_ALLOC_TYPED] = ITEM_OBJECT,
    [UACPI_PARSE_OP_OBJECT_CONVERT_TO_SHALLOW_COPY] = ITEM_OBJECT,
    [UACPI_PARSE_OP_RECORD_AML_PC] = ITEM_IMMEDIATE,
};

static const uacpi_u8 *op_decode_cursor(const struct op_context *ctx)
{
    return &ctx->op->decode_ops[ctx->pc];
}

static uacpi_u8 op_decode_byte(struct op_context *ctx)
{
    uacpi_u8 byte;

    byte = *op_decode_cursor(ctx);
    ctx->pc++;

    return byte;
}

// MSVC doesn't support __VA_OPT__ so we do this weirdness
#define EXEC_OP_DO_WARN(reason, ...)                                 \
    uacpi_kernel_log(UACPI_LOG_WARN, "Op 0x%04X ('%s'): "reason"\n", \
                     op_ctx->op->code, op_ctx->op->name __VA_ARGS__)

#define EXEC_OP_WARN_2(reason, arg0, arg1) EXEC_OP_DO_WARN(reason, ,arg0, arg1)
#define EXEC_OP_WARN_1(reason, arg0) EXEC_OP_DO_WARN(reason, ,arg0)
#define EXEC_OP_WARN(reason) EXEC_OP_DO_WARN(reason)

#define SPEC_SIMPLE_NAME "SimpleName := NameString | ArgObj | LocalObj"
#define SPEC_SUPER_NAME \
    "SuperName := SimpleName | DebugObj | ReferenceTypeOpcode"
#define SPEC_TERM_ARG \
    "TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj"
#define SPEC_OPERAND "Operand := TermArg => Integer"
#define SPEC_TARGET "Target := SuperName | NullName"

static uacpi_bool op_wants_supername(enum uacpi_parse_op op)
{
    switch (op) {
    case UACPI_PARSE_OP_SIMPLE_NAME:
    case UACPI_PARSE_OP_SUPERNAME:
    case UACPI_PARSE_OP_SUPERNAME_IMPLICIT_DEREF:
    case UACPI_PARSE_OP_TARGET:
        return UACPI_TRUE;
    default:
        return UACPI_FALSE;
    }
}

static uacpi_status op_typecheck(const struct op_context *op_ctx,
                                 const struct op_context *cur_op_ctx)
{
    const char *expected_type_str;
    uacpi_u8 ok_mask = 0;
    uacpi_u8 props = cur_op_ctx->op->properties;

    switch (*op_decode_cursor(op_ctx)) {
    // SimpleName := NameString | ArgObj | LocalObj
    case UACPI_PARSE_OP_SIMPLE_NAME:
        expected_type_str = SPEC_SIMPLE_NAME;
        ok_mask |= UACPI_OP_PROPERTY_SIMPLE_NAME;
        break;

    // Target := SuperName | NullName
    case UACPI_PARSE_OP_TARGET:
        expected_type_str = SPEC_TARGET;
        ok_mask |= UACPI_OP_PROPERTY_TARGET | UACPI_OP_PROPERTY_SUPERNAME;
        break;

    // SuperName := SimpleName | DebugObj | ReferenceTypeOpcode
    case UACPI_PARSE_OP_SUPERNAME:
    case UACPI_PARSE_OP_SUPERNAME_IMPLICIT_DEREF:
        expected_type_str = SPEC_SUPER_NAME;
        ok_mask |= UACPI_OP_PROPERTY_SUPERNAME;
        break;

    // TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj
    case UACPI_PARSE_OP_TERM_ARG:
    case UACPI_PARSE_OP_TERM_ARG_UNWRAP_INTERNAL:
    case UACPI_PARSE_OP_OPERAND:
        expected_type_str = SPEC_TERM_ARG;
        ok_mask |= UACPI_OP_PROPERTY_TERM_ARG;
        break;
    }

    if (!(props & ok_mask)) {
        EXEC_OP_WARN_2("invalid argument: '%s', expected a %s",
                       cur_op_ctx->op->name, expected_type_str);
        return UACPI_STATUS_BAD_BYTECODE;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status typecheck_operand(
    const struct op_context *op_ctx,
    const uacpi_object *obj
)
{
    if (uacpi_likely(obj->type == UACPI_OBJECT_INTEGER))
        return UACPI_STATUS_OK;

    EXEC_OP_WARN_2("invalid argument type: %s, expected a %s",
                   uacpi_object_type_to_string(obj->type), SPEC_OPERAND);
    return UACPI_STATUS_BAD_BYTECODE;
}

static uacpi_status uninstalled_op_handler(struct execution_context *ctx)
{
    struct op_context *op_ctx = ctx->cur_op_ctx;

    EXEC_OP_WARN("no dedicated handler installed");
    return UACPI_STATUS_UNIMPLEMENTED;
}

#define LOCAL_HANDLER_IDX 1
#define ARG_HANDLER_IDX 2
#define STRING_HANDLER_IDX 3
#define BINARY_MATH_HANDLER_IDX 4
#define CONTROL_FLOW_HANDLER_IDX 5
#define CODE_BLOCK_HANDLER_IDX 6
#define RETURN_HANDLER_IDX 7
#define CREATE_METHOD_HANDLER_IDX 8
#define COPY_OBJECT_OR_STORE_HANDLER_IDX 9
#define INC_DEC_HANDLER_IDX 10
#define REF_OR_DEREF_OF_HANDLER_IDX 11
#define LOGICAL_NOT_HANDLER_IDX 12
#define LOGICAL_EQUALITY_HANDLER_IDX 13
#define NAMED_OBJECT_HANDLER_IDX 14

uacpi_status (*op_handlers[])(struct execution_context *ctx) = {
    /*
     * All OPs that don't have a handler dispatch to here if
     * UACPI_PARSE_OP_INVOKE_HANDLER is reached.
     */
    [0] = uninstalled_op_handler,
    [LOCAL_HANDLER_IDX] = handle_local,
    [ARG_HANDLER_IDX] = handle_arg,
    [NAMED_OBJECT_HANDLER_IDX] = handle_named_object,
    [STRING_HANDLER_IDX] = handle_string,
    [BINARY_MATH_HANDLER_IDX] = handle_binary_math,
    [CONTROL_FLOW_HANDLER_IDX] = handle_control_flow,
    [CODE_BLOCK_HANDLER_IDX] = handle_code_block,
    [RETURN_HANDLER_IDX] = handle_return,
    [CREATE_METHOD_HANDLER_IDX] = handle_create_method,
    [COPY_OBJECT_OR_STORE_HANDLER_IDX] = handle_copy_object_or_store,
    [INC_DEC_HANDLER_IDX] = handle_inc_dec,
    [REF_OR_DEREF_OF_HANDLER_IDX] = handle_ref_or_deref_of,
    [LOGICAL_NOT_HANDLER_IDX] = handle_logical_not,
    [LOGICAL_EQUALITY_HANDLER_IDX] = handle_logical_equality,
};

static uacpi_u8 handler_idx_of_op[0x100] = {
    [UACPI_AML_OP_Local0Op] = LOCAL_HANDLER_IDX,
    [UACPI_AML_OP_Local1Op] = LOCAL_HANDLER_IDX,
    [UACPI_AML_OP_Local2Op] = LOCAL_HANDLER_IDX,
    [UACPI_AML_OP_Local3Op] = LOCAL_HANDLER_IDX,
    [UACPI_AML_OP_Local4Op] = LOCAL_HANDLER_IDX,
    [UACPI_AML_OP_Local5Op] = LOCAL_HANDLER_IDX,
    [UACPI_AML_OP_Local6Op] = LOCAL_HANDLER_IDX,
    [UACPI_AML_OP_Local7Op] = LOCAL_HANDLER_IDX,

    [UACPI_AML_OP_Arg0Op] = ARG_HANDLER_IDX,
    [UACPI_AML_OP_Arg1Op] = ARG_HANDLER_IDX,
    [UACPI_AML_OP_Arg2Op] = ARG_HANDLER_IDX,
    [UACPI_AML_OP_Arg3Op] = ARG_HANDLER_IDX,
    [UACPI_AML_OP_Arg4Op] = ARG_HANDLER_IDX,
    [UACPI_AML_OP_Arg5Op] = ARG_HANDLER_IDX,
    [UACPI_AML_OP_Arg6Op] = ARG_HANDLER_IDX,

    [UACPI_AML_OP_StringPrefix] = STRING_HANDLER_IDX,

    [UACPI_AML_OP_AddOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_SubtractOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_MultiplyOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_DivideOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_ShiftLeftOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_ShiftRightOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_AndOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_NandOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_OrOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_NorOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_XorOp] = BINARY_MATH_HANDLER_IDX,
    [UACPI_AML_OP_ModOp] = BINARY_MATH_HANDLER_IDX,

    [UACPI_AML_OP_IfOp] = CODE_BLOCK_HANDLER_IDX,
    [UACPI_AML_OP_ElseOp] = CODE_BLOCK_HANDLER_IDX,
    [UACPI_AML_OP_WhileOp] = CODE_BLOCK_HANDLER_IDX,
    [UACPI_AML_OP_ScopeOp] = CODE_BLOCK_HANDLER_IDX,

    [UACPI_AML_OP_ContinueOp] = CONTROL_FLOW_HANDLER_IDX,
    [UACPI_AML_OP_BreakOp] = CONTROL_FLOW_HANDLER_IDX,

    [UACPI_AML_OP_ReturnOp] = RETURN_HANDLER_IDX,

    [UACPI_AML_OP_MethodOp] = CREATE_METHOD_HANDLER_IDX,

    [UACPI_AML_OP_StoreOp] = COPY_OBJECT_OR_STORE_HANDLER_IDX,
    [UACPI_AML_OP_CopyObjectOp] = COPY_OBJECT_OR_STORE_HANDLER_IDX,

    [UACPI_AML_OP_IncrementOp] = INC_DEC_HANDLER_IDX,
    [UACPI_AML_OP_DecrementOp] = INC_DEC_HANDLER_IDX,

    [UACPI_AML_OP_RefOfOp] = REF_OR_DEREF_OF_HANDLER_IDX,
    [UACPI_AML_OP_DerefOfOp] = REF_OR_DEREF_OF_HANDLER_IDX,

    [UACPI_AML_OP_LnotOp] = LOGICAL_NOT_HANDLER_IDX,

    [UACPI_AML_OP_LEqualOp] = LOGICAL_EQUALITY_HANDLER_IDX,

    [UACPI_AML_OP_InternalOpNamedObject] = NAMED_OBJECT_HANDLER_IDX,
};

static uacpi_status exec_op(struct execution_context *ctx)
{
    uacpi_status ret = UACPI_STATUS_OK;
    struct call_frame *frame = ctx->cur_frame;
    struct op_context *op_ctx;
    struct item *item = UACPI_NULL;
    enum uacpi_parse_op prev_op = 0, op;

    /*
     * Allocate a new op context if previous is preempted (looking for a
     * dynamic argument), or doesn't exist at all.
     */
    if (!ctx_has_non_preempted_op(ctx)) {
        ret = push_op(ctx);
        if (uacpi_unlikely_error(ret))
            return ret;
    }

    if (ctx->prev_op_ctx)
        prev_op = *op_decode_cursor(ctx->prev_op_ctx);

    for (;;) {
        if (uacpi_unlikely_error(ret))
            return ret;

        op_ctx = ctx->cur_op_ctx;

        if (op_ctx->pc == 0 && ctx->prev_op_ctx) {
            /*
             * Type check the current arg type against what is expected by the
             * preempted op. This check is able to catch most type violations
             * with the only exception being Operand as we only know whether
             * that evaluates to an integer after the fact.
             */
            ret = op_typecheck(ctx->prev_op_ctx, ctx->cur_op_ctx);
            if (uacpi_unlikely_error(ret))
                return ret;
        }

        op = op_decode_byte(op_ctx);

        if (parse_op_generates_item[op] != ITEM_NONE) {
            item = item_array_alloc(&op_ctx->items);
            if (uacpi_unlikely(item == UACPI_NULL))
                return UACPI_STATUS_OUT_OF_MEMORY;

            item->type = parse_op_generates_item[op];
            if (item->type == ITEM_OBJECT) {
                enum uacpi_object_type type = UACPI_OBJECT_UNINITIALIZED;

                if (op == UACPI_PARSE_OP_OBJECT_ALLOC_TYPED)
                    type = op_decode_byte(op_ctx);

                item->obj = uacpi_create_object(type);
                if (uacpi_unlikely(item->obj == UACPI_NULL))
                    return UACPI_STATUS_OUT_OF_MEMORY;
            }
        } else if (item == UACPI_NULL) {
            item = item_array_last(&op_ctx->items);
        }

        switch (op) {
        case UACPI_PARSE_OP_END: {
            if (op_ctx->tracked_pkg_idx) {
                item = item_array_at(&op_ctx->items, op_ctx->tracked_pkg_idx - 1);
                frame->code_offset = item->pkg.end;
            }

            pop_op(ctx);
            if (ctx->cur_op_ctx) {
                ctx->cur_op_ctx->preempted = UACPI_FALSE;
                ctx->cur_op_ctx->pc++;
            }

            return UACPI_STATUS_OK;
        }

        case UACPI_PARSE_OP_SIMPLE_NAME:
        case UACPI_PARSE_OP_SUPERNAME:
        case UACPI_PARSE_OP_SUPERNAME_IMPLICIT_DEREF:
        case UACPI_PARSE_OP_TERM_ARG:
        case UACPI_PARSE_OP_TERM_ARG_UNWRAP_INTERNAL:
        case UACPI_PARSE_OP_OPERAND:
        case UACPI_PARSE_OP_TARGET:
            /*
             * Preempt this op parsing for now as we wait for the dynamic arg
             * to be parsed.
             */
            op_ctx->preempted = UACPI_TRUE;
            op_ctx->pc--;
            return UACPI_STATUS_OK;

        case UACPI_PARSE_OP_TRACKED_PKGLEN:
            op_ctx->tracked_pkg_idx = item_array_size(&op_ctx->items);
        case UACPI_PARSE_OP_PKGLEN:
            ret = parse_package_length(frame, &item->pkg);
            break;

        case UACPI_PARSE_OP_LOAD_INLINE_IMM_AS_OBJECT:
            item->obj->type = UACPI_OBJECT_INTEGER;
            uacpi_memcpy(
                &item->obj->integer,
                op_decode_cursor(op_ctx),
                8
            );
            op_ctx->pc += 8;
            break;

        case UACPI_PARSE_OP_LOAD_IMM:
        case UACPI_PARSE_OP_LOAD_IMM_AS_OBJECT: {
            uacpi_u8 width;
            void *dst;

            width = op_decode_byte(op_ctx);
            if (uacpi_unlikely(call_frame_code_bytes_left(frame) < width))
                return UACPI_STATUS_BAD_BYTECODE;

            if (op == UACPI_PARSE_OP_LOAD_IMM_AS_OBJECT) {
                item->obj->type = UACPI_OBJECT_INTEGER;
                item->obj->integer = 0;
                dst = &item->obj->integer;
            } else {
                item->immediate = 0;
                dst = item->immediate_bytes;
            }

            uacpi_memcpy(dst, call_frame_cursor(frame), width);
            frame->code_offset += width;
            break;
        }

        case UACPI_PARSE_OP_RECORD_AML_PC:
            item->immediate = frame->code_offset;
            break;

        case UACPI_PARSE_OP_TRUNCATE_NUMBER:
            truncate_number_if_needed(item->obj);
            break;

        case UACPI_PARSE_OP_SET_OBJECT_TYPE:
            item->obj->type = op_decode_byte(op_ctx);
            break;

        case UACPI_PARSE_OP_TYPECHECK: {
            enum uacpi_object_type expected_type;

            expected_type = op_decode_byte(op_ctx);

            if (uacpi_unlikely(item->obj->type != expected_type)) {
                EXEC_OP_WARN_2("bad object type: expected %d, got %d!",
                               expected_type, item->obj->type);
                ret = UACPI_STATUS_BAD_BYTECODE;
            }

            break;
        }

        case UACPI_PARSE_OP_TODO:
            EXEC_OP_WARN("not yet implemented");
            ret = UACPI_STATUS_UNIMPLEMENTED;
            break;

        case UACPI_PARSE_OP_BAD_OPCODE:
        case UACPI_PARSE_OP_UNREACHABLE:
            EXEC_OP_WARN("invalid/unexpected opcode");
            ret = UACPI_STATUS_BAD_BYTECODE;
            break;

        case UACPI_PARSE_OP_AML_PC_DECREMENT:
            frame->code_offset--;
            break;

        case UACPI_PARSE_OP_CREATE_NAMESTRING:
        case UACPI_PARSE_OP_EXISTING_NAMESTRING: {
            enum resolve_behavior behavior;

            if (op == UACPI_PARSE_OP_CREATE_NAMESTRING)
                behavior = RESOLVE_CREATE_LAST_NAMESEG_FAIL_IF_EXISTS;
            else
                behavior = RESOLVE_FAIL_IF_DOESNT_EXIST;

            ret = resolve_name_string(frame, behavior, &item->node);
            break;
        }

        case UACPI_PARSE_OP_INVOKE_HANDLER:
            ret = op_handlers[handler_idx_of_op[op_ctx->op->code]](ctx);
            break;

        case UACPI_PARSE_OP_INSTALL_NAMESPACE_NODE:
            item = item_array_at(&op_ctx->items, op_decode_byte(op_ctx));
            ret = uacpi_node_install(item->node->parent, item->node);
            break;

        case UACPI_PARSE_OP_OBJECT_TRANSFER_TO_PREV:
        case UACPI_PARSE_OP_OBJECT_COPY_TO_PREV: {
            uacpi_object *src;
            struct item *dst;

            if (!ctx->prev_op_ctx)
                break;

            switch (prev_op) {
            case UACPI_PARSE_OP_TERM_ARG_UNWRAP_INTERNAL:
            case UACPI_PARSE_OP_OPERAND:
                src = object_deref_if_internal(item->obj);

                if (prev_op == UACPI_PARSE_OP_OPERAND)
                    ret = typecheck_operand(ctx->prev_op_ctx, src);

                break;
            case UACPI_PARSE_OP_SUPERNAME:
            case UACPI_PARSE_OP_SUPERNAME_IMPLICIT_DEREF:
                if (prev_op == UACPI_PARSE_OP_SUPERNAME_IMPLICIT_DEREF)
                    src = object_deref_implicit(item->obj);
                else
                    src = item->obj;
                break;

            case UACPI_PARSE_OP_SIMPLE_NAME:
            case UACPI_PARSE_OP_TERM_ARG:
            case UACPI_PARSE_OP_TARGET:
                src = item->obj;
                break;

            default:
                EXEC_OP_WARN_1("don't know how to copy/transfer object to %d",
                               prev_op);
                ret = UACPI_STATUS_INVALID_ARGUMENT;
                break;
            }

            if (uacpi_likely_success(ret)) {
                dst = item_array_last(&ctx->prev_op_ctx->items);
                dst->type = ITEM_OBJECT;

                if (op == UACPI_PARSE_OP_OBJECT_TRANSFER_TO_PREV) {
                    dst->obj = src;
                    uacpi_object_ref(dst->obj);
                } else {
                    dst->obj = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);
                    if (uacpi_unlikely(dst->obj == UACPI_NULL)) {
                        ret = UACPI_STATUS_OUT_OF_MEMORY;
                        break;
                    }

                    ret = uacpi_object_assign(dst->obj, src,
                                              UACPI_ASSIGN_BEHAVIOR_DEEP_COPY);
                }
            }
            break;
        }

        case UACPI_PARSE_OP_STORE_TO_TARGET:
        case UACPI_PARSE_OP_STORE_TO_TARGET_INDIRECT: {
            uacpi_object *dst, *src;

            dst = item_array_at(&op_ctx->items, op_decode_byte(op_ctx))->obj;

            if (op == UACPI_PARSE_OP_STORE_TO_TARGET_INDIRECT) {
                src = item_array_at(&op_ctx->items,
                                    op_decode_byte(op_ctx))->obj;
            } else {
                src = item->obj;
            }

            ret = store_to_target(dst, src);
            break;
        }

        // Nothing to do here, object is allocated automatically
        case UACPI_PARSE_OP_OBJECT_ALLOC:
        case UACPI_PARSE_OP_OBJECT_ALLOC_TYPED:
            break;


        case UACPI_PARSE_OP_OBJECT_CONVERT_TO_SHALLOW_COPY: {
            uacpi_object *temp = item->obj;

            item_array_pop(&op_ctx->items);
            item = item_array_last(&op_ctx->items);

            ret = uacpi_object_assign(temp, item->obj,
                                      UACPI_ASSIGN_BEHAVIOR_SHALLOW_COPY);
            if (uacpi_unlikely_error(ret))
                break;

            uacpi_object_unref(item->obj);
            item->obj = temp;
            break;
        }

        case UACPI_PARSE_OP_DEREF_IF_INTERNAL: {
            uacpi_object *temp;

            temp = object_deref_if_internal(item->obj);
            uacpi_object_ref(temp);
            uacpi_object_unref(item->obj);
            item->obj = temp;
            break;
        }

        case UACPI_PARSE_OP_DISPATCH_METHOD_CALL: {
            struct uacpi_control_method *method;

            method = item_array_at(&op_ctx->items, 0)->node->object->method;

            ret = push_new_frame(ctx, &frame);
            if (uacpi_unlikely_error(ret))
                return ret;

            frame_push_args(frame, ctx->cur_op_ctx);
            frame_setup_base_scope(frame, method);

            ctx->cur_frame = frame;
            ctx->cur_frame->method = method;
            ctx->cur_op_ctx = UACPI_NULL;
            ctx->prev_op_ctx = UACPI_NULL;
            ctx->cur_block = code_block_array_last(&ctx->cur_frame->code_blocks);
            return UACPI_STATUS_OK;
        }

        case UACPI_PARSE_OP_CONVERT_NAMESTRING: {
            uacpi_aml_op new_op;

            if (prev_op && op_wants_supername(prev_op)) {
                new_op = UACPI_AML_OP_InternalOpNamedObject;
            } else {
                uacpi_object *obj = item->node->object;

                if (obj->type == UACPI_OBJECT_METHOD) {
                    new_op = UACPI_AML_OP_InternalOpMethodCall0Args;
                    new_op += obj->method->args;
                } else {
                    new_op = UACPI_AML_OP_InternalOpNamedObject;
                }
            }

            op_ctx->pc = 0;
            op_ctx->op = uacpi_get_op_spec(new_op);
            break;
        }

        default:
            EXEC_OP_WARN_1("unhandled parser op '%d'", op);
            ret = UACPI_STATUS_UNIMPLEMENTED;
            break;
        }
    }
}

uacpi_status uacpi_execute_control_method(uacpi_control_method *method,
                                          uacpi_args *args, uacpi_object **ret)
{
    uacpi_status st = UACPI_STATUS_OK;
    struct execution_context *ctx;
    struct call_frame *cur_frame;
    struct op *cur_op;

    ctx = uacpi_kernel_calloc(1, sizeof(*ctx));
    if (ctx == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    if (ret != UACPI_NULL) {
        ctx->ret = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);
        if (uacpi_unlikely(ctx->ret == UACPI_NULL)) {
            st = UACPI_STATUS_OUT_OF_MEMORY;
            goto out;
        }
    }

    ctx->cur_method = method;

    ctx->cur_frame = call_frame_array_calloc(&ctx->call_stack);
    if (uacpi_unlikely(ctx->cur_frame == UACPI_NULL)) {
        st = UACPI_STATUS_OUT_OF_MEMORY;
        goto out;
    }
    ctx->cur_frame->method = ctx->cur_method;

    if (args != UACPI_NULL) {
        uacpi_u8 i;

        if (args->count != method->args) {
            st = UACPI_STATUS_INVALID_ARGUMENT;
            goto out;
        }

        for (i = 0; i < method->args; ++i) {
            ctx->cur_frame->args[i] = args->objects[i];
            uacpi_object_ref(args->objects[i]);
        }
    } else if (method->args) {
        st = UACPI_STATUS_INVALID_ARGUMENT;
        goto out;
    }

    frame_setup_base_scope(ctx->cur_frame, method);
    ctx->cur_block = code_block_array_last(&ctx->cur_frame->code_blocks);

    for (;;) {
        if (!ctx_has_non_preempted_op(ctx)) {
            if (ctx->cur_frame == UACPI_NULL)
                break;

            if (maybe_end_block(ctx))
                continue;

            if (!call_frame_has_code(ctx->cur_frame)) {
                ctx_reload_post_ret(ctx);
                continue;
            }

            st = get_op(ctx);
            if (uacpi_unlikely_error(st))
                goto out;

            trace_op(ctx->cur_op);
        }

        st = exec_op(ctx);
        if (uacpi_unlikely_error(st))
            goto out;

        ctx->skip_else = UACPI_FALSE;
    }

out:
    if (ret && ctx->ret->type != UACPI_OBJECT_UNINITIALIZED) {
        uacpi_object_ref(ctx->ret);
        *ret = ctx->ret;
    }
    execution_context_release(ctx);
    return st;
}
