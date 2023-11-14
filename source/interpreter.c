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
 * The implementation of references:
 *
 * Bytecode OPs like ArgX and LocalX are always converted to reference objects
 * for simplicity, the assigned reference kind is REFERENCE_KIND_LOCAL and
 * REFERENCE_KIND_ARG respectively and the referenced object is either
 * a member of call_frame::locals or call_frame::args.
 *
 * A call to RefOf generates a new reference object of type
 * REFERENCE_KIND_NORMAL that references the provided object dereferenced
 * according to rules specified above object_deref_implicit.
 *
 * Now for the more complicated part - dereferencing (implicit or via DerefOf):
 *
 * Every dereference either explicit or implicit has to unwind the reference
 * chain all the way to the bottom, this is done to mimic the implementation
 * used in the NT kernel (which is what all AML code is tested against by
 * default)
 *
 * Let's break down a few examples:
 *
 * 1. Local0 = 123
 * Local0 is converted to a REFERENCE_KIND_LOCAL where the referenced object is
 * set to call_frame->locals[0].
 *
 * DerefOf(Local0) works as following:
 *     1. Dereference the reference to local via object_deref_if_internal.
 *     2. The resulting object is not a reference, this is an error.
 *
 * 2. Local1 = 123; Local0 = RefOf(Local1)
 * In the example above Local0 is broken down as following:
 *     Local0 (UACPI_OBJECT_REFERENCE, REFERENCE_KIND_LOCAL)
 *     |
 *     v
 *     call_frame->locals[0] (UACPI_OBJECT_REFERENCE, REFERENCE_KIND_REFOF)
 *     |
 *     v
 *     call_frame->locals[1] (UACPI_OBJECT_INTEGER)
 *
 * DerefOf(Local0) works as following:
 *     1. Dereference the reference to local via object_deref_if_internal.
 *     2. Start unwinding via unwind_reference()
 *         - Current object is REFERENCE_KIND_REFOF, take the referenced object
*            (call_frame->locals[1])
 *         - Current object is not a reference, so it's the result of
 *           the DerefOf -- we're done.
 *
 * 3. MAIN(123)
 * In this example Arg0 is broken down as following:
 *     Arg0 (UACPI_OBJECT_REFERENCE, REFERENCE_KIND_ARG)
 *     |
 *     v
 *     call_frame->args[0] (UACPI_OBJECT_INTEGER)
 *
 * 4. Local0 = 123; Local1 = RefOf(Local0); MAIN(RefOf(Local1))
 * In this example Arg0 is broken down as following:
 *     Arg0 (UACPI_OBJECT_REFERENCE, REFERENCE_KIND_ARG)
 *     |
 *     v
 *     call_frame->args[0] (UACPI_OBJECT_REFERENCE, REFERENCE_KIND_REFOF)
 *     |
 *     v
 *     prev_call_frame->locals[1] (UACPI_OBJECT_REFERENCE, REFERENCE_KIND_REFOF)
 *     |
 *     v
 *     prev_call_frame->locals[0] (UACPI_OBJECT_INTEGER)
 *
 * DerefOf(Arg0) works as following:
 *     1. Dereference the reference to arg via object_deref_if_internal.
 *     2. Start unwinding via unwind_reference()
 *         - Current object is REFERENCE_KIND_REFOF, take the referenced object
 *           (prev_call_frame->locals[1])
 *         - Current object is REFERENCE_KIND_REFOF, take the referenced object
 *           (prev_call_frame->locals[0])
 *         - Current object is not a reference, so it's the result of
 *           the DerefOf -- we're done.
 *
 * Store(..., ArgX/LocalX) automatically dereferences as if by DerefOf in the
 * example above.
 */

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(operand_array, uacpi_object*, 8)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(operand_array, uacpi_object*, static)

struct op {
    uacpi_aml_op code;
    struct uacpi_opcode_info info;
};

struct pending_op {
    uacpi_aml_op code;
    struct uacpi_opcode_info info;
    struct operand_array operands;
};

static uacpi_bool op_dispatchable(struct pending_op *pop)
{
    uacpi_u8 tgt_count;
    struct uacpi_opcode_info *info = &pop->info;

    switch (info->type) {
    case UACPI_OPCODE_TYPE_EXEC:
        tgt_count = info->as_exec.operand_count;
        break;
    case UACPI_OPCODE_TYPE_FLOW:
        tgt_count = info->as_flow.has_operand;
        break;
    case UACPI_OPCODE_TYPE_METHOD_CALL:
        tgt_count = info->as_method_call.node->object.as_method.method->args;
        break;
    default:
        return UACPI_FALSE;
    }

    return operand_array_size(&pop->operands) == tgt_count;
}

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(pending_op_array, struct pending_op, 4)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(
    pending_op_array, struct pending_op, static
)

enum flow_frame_type {
    FLOW_FRAME_IF = 1,
    FLOW_FRAME_ELSE = 2,
    FLOW_FRAME_WHILE = 3,
};

struct flow_frame {
    enum flow_frame_type type;
    uacpi_u32 begin, end;
};

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(flow_frame_array, struct flow_frame, 6)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(
    flow_frame_array, struct flow_frame, static
)

struct call_frame {
    struct uacpi_control_method *method;
    struct op cur_op;

    uacpi_object *args[7];
    uacpi_object *locals[8];

    /*
     * Each op with operands gets a 'pending op', e.g. for the following code:
     * ---------------------------------------------------------------
     * Return (GETX(GETY(ADD(5, GETZ()))))
     * ---------------------------------------------------------------
     * The op contexts would look like this:
     * cur_pop[0] = ReturnOp, expected_args = 1, args = <pending>
     * cur_pop[1] = MethodCall (GETX), expected_args = 1, args = <pending>
     * cur_pop[2] = MethodCall (GETY), expected_args = 1, args = <pending>
     * cur_pop[3] = AddOp, expected_args = 2, args[0] = 5, args[1] = <pending>
     * GETZ (currently being executed)
     *
     * The idea is that as soon as a 'pending op' gets its
     * arg_count == target_arg_count it is dispatched (aka executed) right
     * away, in a sort of "tetris" way. This allows us to guarantee left to
     * right execution (same as ACPICA) and also zero stack usage as all of
     * this logic happens within one function.
     */
    struct pending_op_array pending_ops;
    struct flow_frame_array flows;
    struct flow_frame *last_while;

    uacpi_u32 code_offset;
};

static uacpi_size op_size(struct op *op)
{
    if ((op->code >> 8) == UACPI_EXT_PREFIX)
        return 2;

    return 1;
}

static void call_frame_advance_pc(struct call_frame *frame)
{
    frame->code_offset += op_size(&frame->cur_op);
}

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

struct execution_context {
    uacpi_object *ret;
    struct call_frame_array call_stack;

    struct call_frame *cur_frame;
    struct flow_frame *cur_flow;
    struct uacpi_control_method *cur_method;
    struct pending_op *cur_pop;

    uacpi_bool skip_else;
};

#define AML_READ(ptr, offset) (*(((uacpi_u8*)(code)) + offset))

static uacpi_status parse_name(struct call_frame *frame,
                               uacpi_object_name *out_name)
{
    uacpi_size i;
    uacpi_char *cursor;

    i = call_frame_code_bytes_left(frame);
    if (uacpi_unlikely(i < 4))
        return UACPI_STATUS_BAD_BYTECODE;

    /*
     * This is all we support for now:
     *‘A’-‘Z’ := 0x41 - 0x5A
     * ‘_’ := 0x5F
     *‘0’-‘9’ := 0x30 - 0x39
     */

    cursor = call_frame_cursor(frame);
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
    frame->code_offset += 4;
    return UACPI_STATUS_OK;
}

static uacpi_status resolve_method_call(struct call_frame *frame)
{
    uacpi_status ret;
    uacpi_object_name name;
    struct uacpi_opcode_method_call *mc = &frame->cur_op.info.as_method_call;

    ret = parse_name(frame, &name);
    if (uacpi_unlikely_error(ret))
        return ret;

    mc->node = uacpi_namespace_node_find(UACPI_NULL, name);
    if (mc->node == UACPI_NULL)
        return UACPI_STATUS_NOT_FOUND;

    return UACPI_STATUS_OK;
}

static uacpi_bool is_op_method_call(uacpi_aml_op op)
{
    return op == '\\' || op == '/' || op == '.' ||
          (op >= 'A' && op <= 'Z');
}

// Temporary until we have a proper opcode table
static struct uacpi_opcode_info *opcode_table_find_op(uacpi_aml_op op)
{
    uacpi_size i;

    for (i = 0; uacpi_opcode_table[i].name; ++i) {
        if (uacpi_opcode_table[i].code == op) {
            return &uacpi_opcode_table[i];
        }
    }

    uacpi_kernel_log(UACPI_LOG_WARN, "Unimplemented opcode 0x%016X\n", op);
    return UACPI_NULL;
}

uacpi_status peek_op(struct call_frame *frame)
{
    uacpi_aml_op op;
    void *code = frame->method->code;
    uacpi_size size = frame->method->size, offset = frame->code_offset;
    struct uacpi_opcode_info *info;

    if (uacpi_unlikely(offset >= size))
        return UACPI_STATUS_OUT_OF_BOUNDS;

    op = AML_READ(code, offset++);
    if (op == UACPI_EXT_PREFIX) {
        if (uacpi_unlikely(offset >= size))
            return UACPI_STATUS_OUT_OF_BOUNDS;

        op <<= 8;
        op |= AML_READ(code, offset);
    } else if (is_op_method_call(op)) {
        op = UACPI_AML_OP_UACPIInternalOpMethodCall;
    }

    info = opcode_table_find_op(op);
    if (info == UACPI_NULL)
        return UACPI_STATUS_UNIMPLEMENTED;

    frame->cur_op.code = op;
    frame->cur_op.info = *info;

    if (op == UACPI_AML_OP_UACPIInternalOpMethodCall) {
        return resolve_method_call(frame);
    }

    return UACPI_STATUS_OK;
}

uacpi_status pop_operand_alloc(struct pending_op *pop, uacpi_object **out_operand)
{
    uacpi_object **operand;

    operand = operand_array_alloc(&pop->operands);
    if (operand == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    *operand = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);
    if (*operand == UACPI_NULL) {
        operand_array_pop(&pop->operands);
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    *out_operand = *operand;
    return UACPI_STATUS_OK;
}

uacpi_status next_arg(struct call_frame *frame, uacpi_object **out_operand)
{
    struct pending_op *pop;

    pop = pending_op_array_last(&frame->pending_ops);
    // Just a stray argument in the bytecode
    if (pop == UACPI_NULL)
        return UACPI_STATUS_BAD_BYTECODE;

    return pop_operand_alloc(pop, out_operand);
}

uacpi_status get_string(struct call_frame *frame)
{
    uacpi_status ret;
    uacpi_object *obj;
    char *string;
    size_t length;

    ret = next_arg(frame, &obj);
    if (uacpi_unlikely_error(ret))
        return ret;

    string = call_frame_cursor(frame);

    // TODO: sanitize string for valid UTF-8
    length = uacpi_strnlen(string, call_frame_code_bytes_left(frame));

    if (string[length++] != 0x00)
        return UACPI_STATUS_BAD_BYTECODE;

    obj->as_string.type = UACPI_OBJECT_STRING;
    obj->as_string.text = uacpi_kernel_alloc(length);
    if (uacpi_unlikely(obj->as_string.text == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    uacpi_memcpy(obj->as_string.text, string, length);
    obj->as_string.length = length;
    frame->code_offset += length;
    return UACPI_STATUS_OK;
}

static uacpi_status copy_buffer(uacpi_object *dst, uacpi_object *src)
{
    struct uacpi_object_buffer *src_buf = &src->as_buffer;
    struct uacpi_object_buffer *dst_buf = &dst->as_buffer;

    if (src->common.refcount == 1) {
        dst_buf->data = src_buf->data;
        dst_buf->size = src_buf->size;
        src_buf->data = UACPI_NULL;
        src_buf->size = 0;
    } else {
        dst_buf->data = uacpi_kernel_alloc(src_buf->size);
        if (uacpi_unlikely(dst_buf->data == UACPI_NULL))
            return UACPI_STATUS_OUT_OF_MEMORY;

        dst_buf->size = src_buf->size;
        uacpi_memcpy(dst_buf->data, src_buf->data, src_buf->size);
    }

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
        out_buf->ptr = &obj->as_integer.value;
        break;
    case UACPI_OBJECT_STRING:
        out_buf->len = obj->as_string.length ? obj->as_string.length - 1 : 0;
        out_buf->ptr = obj->as_string.text;
        break;
    case UACPI_OBJECT_BUFFER:
        out_buf->ptr = obj->as_buffer.data;
        out_buf->len = obj->as_buffer.size;
        break;
    case UACPI_OBJECT_REFERENCE:
        return UACPI_STATUS_INVALID_ARGUMENT;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
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

static uacpi_status object_overwrite_try_elide(uacpi_object *dst,
                                               uacpi_object *src)
{
    uacpi_status ret = UACPI_STATUS_OK;

    if (dst->type == UACPI_OBJECT_REFERENCE) {
        uacpi_u32 refs_to_remove = dst->common.refcount;
        while (refs_to_remove--)
            uacpi_object_unref(dst->as_reference.object);
    } else if (dst->type == UACPI_OBJECT_STRING ||
               dst->type == UACPI_OBJECT_BUFFER) {
        uacpi_kernel_free(dst->as_buffer.data);
        dst->as_buffer.data = NULL;
        dst->as_buffer.size = 0;
    }

    switch (src->type) {
    case UACPI_OBJECT_UNINITIALIZED:
        break;
    case UACPI_OBJECT_BUFFER:
    case UACPI_OBJECT_STRING:
        ret = copy_buffer(dst, src);
        break;
    case UACPI_OBJECT_INTEGER:
        dst->as_integer.value = src->as_integer.value;
        break;
    case UACPI_OBJECT_METHOD:
        dst->as_method.method = src->as_method.method;
        break;
    case UACPI_OBJECT_SPECIAL:
        dst->as_special.special_type = src->as_special.special_type;
        break;
    case UACPI_OBJECT_REFERENCE: {
        uacpi_u32 refs_to_add = dst->common.refcount;
        uacpi_object_reference *ref = &dst->as_reference;

        dst->common.flags = src->common.flags;
        ref->object = src->as_reference.object;

        while (refs_to_add-- > 0)
            uacpi_object_ref(ref->object);
        break;
    } default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
    }

    if (ret == UACPI_STATUS_OK)
        dst->type = src->type;

    return ret;
}

static uacpi_object *object_deref_if_internal(uacpi_object *object)
{
    for (;;) {
        if (object->type != UACPI_OBJECT_REFERENCE ||
            object->common.flags == REFERENCE_KIND_REFOF)
            return object;

        object = object->as_reference.object;
    }
}

static uacpi_status copy_retval(uacpi_object *dst, uacpi_object *src)
{
    return object_overwrite_try_elide(dst, object_deref_if_internal(src));
}

static uacpi_status get_arg_or_local_ref(
    struct call_frame *frame, enum uacpi_arg_sub_type sub_type
)
{
    uacpi_status ret;
    uacpi_object **src, *dst;
    enum reference_kind kind;

    switch (sub_type) {
    case UACPI_ARG_SUB_TYPE_LOCAL:
        src = &frame->locals[frame->cur_op.code - UACPI_AML_OP_Local0Op];
        kind = REFERENCE_KIND_LOCAL;
        break;
    case UACPI_ARG_SUB_TYPE_ARG:
        src = &frame->args[frame->cur_op.code - UACPI_AML_OP_Arg0Op];
        kind = REFERENCE_KIND_ARG;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    ret = next_arg(frame, &dst);
    if (uacpi_unlikely_error(ret))
        return ret;

    // Access to an uninitialized local or arg, hopefully a store incoming
    if (*src == UACPI_NULL) {
        *src = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);
        if (uacpi_unlikely(*src == UACPI_NULL))
            return UACPI_STATUS_OUT_OF_MEMORY;
    }

    dst->common.flags = kind;
    dst->type = UACPI_OBJECT_REFERENCE;
    dst->as_reference.object = *src;
    uacpi_object_ref(*src);

    return ret;
}

static void truncate_number_if_needed(uacpi_object *obj)
{
    if (!g_uacpi_rt_ctx.is_rev1)
        return;

    obj->as_integer.value &= 0xFFFFFFFF;
}

static uacpi_u64 ones()
{
    return g_uacpi_rt_ctx.is_rev1 ? 0xFFFFFFFF : 0xFFFFFFFFFFFFFFFF;
}

uacpi_status get_number(struct call_frame *frame)
{
    uacpi_status ret;
    uacpi_object *obj;
    void *data;
    uacpi_u8 bytes;

    ret = next_arg(frame, &obj);
    if (uacpi_unlikely_error(ret))
        return ret;

    data = call_frame_cursor(frame);

    switch (frame->cur_op.code) {
    case UACPI_AML_OP_ZeroOp:
        obj->as_integer.value = 0;
        goto out;
    case UACPI_AML_OP_OneOp:
        obj->as_integer.value = 1;
        goto out;
    case UACPI_AML_OP_OnesOp:
        obj->as_integer.value = ones();
        goto out;
    case UACPI_AML_OP_BytePrefix:
        bytes = 1;
        break;
    case UACPI_AML_OP_WordPrefix:
        bytes = 2;
        break;
    case UACPI_AML_OP_DWordPrefix:
        bytes = 4;
        break;
    case UACPI_AML_OP_QWordPrefix:
        bytes = 8;
        break;
    }

    if (call_frame_code_bytes_left(frame) < bytes)
        return UACPI_STATUS_BAD_BYTECODE;

    obj->as_integer.value = 0;
    uacpi_memcpy(&obj->as_integer.value, data, bytes);
    truncate_number_if_needed(obj);
    frame->code_offset += bytes;

out:
    obj->type = UACPI_OBJECT_INTEGER;
    return UACPI_STATUS_OK;
}

uacpi_status get_special(struct call_frame *frame)
{
    uacpi_status ret;
    uacpi_object *obj;

    ret = next_arg(frame, &obj);
    if (uacpi_unlikely_error(ret))
        return ret;

    switch (frame->cur_op.code) {
    case UACPI_AML_OP_DebugOp:
        obj->as_special.type = UACPI_OBJECT_SPECIAL;
        obj->as_special.special_type = UACPI_SPECIAL_TYPE_DEBUG_OBJECT;
        return UACPI_STATUS_OK;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

static uacpi_status method_get_ret_target(struct execution_context *ctx,
                                          uacpi_object **out_operand)
{
    uacpi_size depth;

    // Check if we're targeting the previous call frame
    depth = call_frame_array_size(&ctx->call_stack);
    if (depth > 1) {
        struct pending_op *pop;
        struct call_frame *frame;

        frame = call_frame_array_at(&ctx->call_stack, depth - 2);
        depth = pending_op_array_size(&frame->pending_ops);

        // Ok, no one wants the return value at call site. Discard it.
        if (!depth) {
            *out_operand = UACPI_NULL;
            return UACPI_STATUS_OK;
        }

        pop = pending_op_array_at(&frame->pending_ops, depth - 1);
        return pop_operand_alloc(pop, out_operand);
    }

    return UACPI_STATUS_NOT_FOUND;
}

static uacpi_status exec_get_ret_target(struct execution_context *ctx,
                                        uacpi_object **out_operand)
{
    uacpi_size depth;
    struct pending_op_array *pops = &ctx->cur_frame->pending_ops;

    // Check if we have a pending op looking for args
    depth = pending_op_array_size(pops);
    if (depth > 1) {
        struct pending_op *pop;

        pop = pending_op_array_at(pops, depth - 2);
        return pop_operand_alloc(pop, out_operand);
    }

    *out_operand = UACPI_NULL;
    return UACPI_STATUS_OK;
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

static uacpi_status begin_flow_execution(struct execution_context *ctx)
{
    struct call_frame *cur_frame = ctx->cur_frame;
    struct uacpi_opcode_flow *op;
    struct flow_frame *flow_frame;

    flow_frame = flow_frame_array_alloc(&cur_frame->flows);
    if (uacpi_unlikely(flow_frame == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    switch (ctx->cur_pop->code) {
    case UACPI_AML_OP_IfOp:
        flow_frame->type = FLOW_FRAME_IF;
        break;
    case UACPI_AML_OP_ElseOp:
        flow_frame->type = FLOW_FRAME_ELSE;
        break;
    case UACPI_AML_OP_WhileOp:
        flow_frame->type = FLOW_FRAME_WHILE;
        cur_frame->last_while = flow_frame;
        break;
    default:
        flow_frame_array_pop(&cur_frame->flows);
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    op = &ctx->cur_pop->info.as_flow;
    flow_frame->begin = op->start_offset;
    flow_frame->end = op->end_offset;
    ctx->cur_flow = flow_frame;
    return UACPI_STATUS_OK;
}

static uacpi_status handle_predicate_result(struct execution_context *ctx,
                                            uacpi_bool result)
{
    if (result)
        return begin_flow_execution(ctx);

    ctx->cur_frame->code_offset = ctx->cur_pop->info.as_flow.end_offset;
    return UACPI_STATUS_OK;
}

static uacpi_status predicate_evaluate(uacpi_object *operand, uacpi_bool *res)
{
    uacpi_object *unwrapped_obj;

    unwrapped_obj = object_deref_if_internal(operand);
    if (unwrapped_obj->type != UACPI_OBJECT_INTEGER)
        return UACPI_STATUS_BAD_BYTECODE;

    *res = unwrapped_obj->as_integer.value != 0;
    return UACPI_STATUS_OK;
}

static struct flow_frame *find_last_while_flow(struct flow_frame_array *flows)
{
    uacpi_size i;

    i = flow_frame_array_size(flows);
    while (i-- > 0) {
        struct flow_frame *flow;

        flow = flow_frame_array_at(flows, i);
        if (flow->type == FLOW_FRAME_WHILE)
            return flow;
    }

    return UACPI_NULL;
}

static void frame_reset_post_end_flow(struct execution_context *ctx,
                                      uacpi_bool reset_last_while)
{
    struct call_frame *frame = ctx->cur_frame;
    flow_frame_array_pop(&frame->flows);
    ctx->cur_flow = flow_frame_array_last(&frame->flows);

    if (reset_last_while)
        frame->last_while = find_last_while_flow(&frame->flows);
}

static uacpi_status flow_dispatch(struct execution_context *ctx)
{
    uacpi_status ret;
    struct call_frame *cur_frame = ctx->cur_frame;
    struct pending_op *pop = ctx->cur_pop;

    switch (pop->code) {
    case UACPI_AML_OP_ContinueOp:
    case UACPI_AML_OP_BreakOp: {
        struct flow_frame *flow;

        for (;;) {
            flow = flow_frame_array_last(&cur_frame->flows);
            if (flow != cur_frame->last_while) {
                flow_frame_array_pop(&cur_frame->flows);
                continue;
            }

            if (pop->code == UACPI_AML_OP_BreakOp)
                cur_frame->code_offset = flow->end;
            else
                cur_frame->code_offset = flow->begin;
            frame_reset_post_end_flow(ctx, UACPI_TRUE);
            return UACPI_STATUS_OK;
        }
    }
    case UACPI_AML_OP_ReturnOp: {
        uacpi_object *dst = UACPI_NULL;

        cur_frame->code_offset = cur_frame->method->size;
        ret = method_get_ret_object(ctx, &dst);

        if (uacpi_unlikely_error(ret))
            return ret;
        if (dst == UACPI_NULL)
            return UACPI_STATUS_OK;

        return copy_retval(dst, *operand_array_at(&pop->operands, 0));
    }
    case UACPI_AML_OP_ElseOp:
        return begin_flow_execution(ctx);
    case UACPI_AML_OP_IfOp:
    case UACPI_AML_OP_WhileOp: {
        uacpi_bool res;

        ret = predicate_evaluate(*operand_array_at(&pop->operands, 0), &res);
        if (uacpi_unlikely_error(ret))
            return ret;

        return handle_predicate_result(ctx, res);
    }
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

static uacpi_status special_store(uacpi_object *dst, uacpi_object *src)
{
    if (dst->as_special.special_type != UACPI_SPECIAL_TYPE_DEBUG_OBJECT)
        return UACPI_STATUS_INVALID_ARGUMENT;

    src = object_deref_if_internal(src);

    switch (src->type) {
    case UACPI_OBJECT_UNINITIALIZED:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Uninitialized]\n");
        break;
    case UACPI_OBJECT_STRING:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, String] %s\n",
                         src->as_string.text);
        break;
    case UACPI_OBJECT_INTEGER:
        if (g_uacpi_rt_ctx.is_rev1) {
            uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Integer] 0x%08llX\n",
                             src->as_integer.value);
        } else {
            uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Integer] 0x%016llX\n",
                             src->as_integer.value);
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

/*
 * NOTE: this function returns the slot in the parent object at which the
 *       child object is stored.
 */
uacpi_object **reference_unwind(uacpi_object *obj)
{
    uacpi_object *parent = obj;

    while (obj) {
        if (obj->type != UACPI_OBJECT_REFERENCE)
            return &parent->as_reference.object;

        parent = obj;
        obj = parent->as_reference.object;
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
 *
 * NOTE: this function returns the slot in the parent object at which the
 *       child object is stored.
 */
static uacpi_object **object_deref_implicit(uacpi_object *obj)
{
    if (obj->common.flags != REFERENCE_KIND_REFOF) {
        if (obj->common.flags == REFERENCE_KIND_NAMED ||
            obj->as_reference.object->type != UACPI_OBJECT_REFERENCE)
            return &obj->as_reference.object;

        obj = obj->as_reference.object;
    }

    return reference_unwind(obj);
}

/*
 * Explicit dereferencing [DerefOf] behavior:
 * Simply grabs the bottom-most object that is not a reference.
 * This mimics the behavior of NT Acpi.sys: any DerfOf fetches
 * the bottom-most reference. Note that this is different from
 * ACPICA where DerefOf dereferences one level.
 */
static uacpi_status object_deref_explicit(uacpi_object *obj,
                                          uacpi_object **out_obj)
{
    obj = object_deref_if_internal(obj);

    if (obj->type != UACPI_OBJECT_REFERENCE)
        return UACPI_STATUS_BAD_BYTECODE;

    *out_obj = *reference_unwind(obj);
    return UACPI_STATUS_OK;
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
    uacpi_object **dst_slot;
    uacpi_object *src_obj;

    switch (dst->common.flags) {
    case REFERENCE_KIND_ARG: {
        uacpi_object *referenced_obj;

        referenced_obj = object_deref_if_internal(dst);
        if (referenced_obj->type == UACPI_OBJECT_REFERENCE &&
            referenced_obj->common.flags == REFERENCE_KIND_REFOF) {
            dst_slot = reference_unwind(referenced_obj);
            break;
        }

        // FALLTHROUGH intended here
    }
    case REFERENCE_KIND_LOCAL:
    case REFERENCE_KIND_NAMED:
        dst_slot = &dst->as_reference.object;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    src_obj = object_deref_if_internal(src);
    return object_overwrite_try_elide(*dst_slot, src_obj);
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
    uacpi_object **dst_slot;
    uacpi_object *src_obj;
    uacpi_bool overwrite = UACPI_FALSE;

    switch (dst->common.flags) {
    case REFERENCE_KIND_LOCAL:
    case REFERENCE_KIND_ARG: {
        uacpi_object *referenced_obj;

        referenced_obj = object_deref_if_internal(dst);
        if (referenced_obj->type == UACPI_OBJECT_REFERENCE &&
            referenced_obj->common.flags == REFERENCE_KIND_REFOF) {
            dst_slot = reference_unwind(referenced_obj);
            overwrite = dst->common.flags == REFERENCE_KIND_ARG;
            break;
        }

        overwrite = UACPI_TRUE;
        dst_slot = &dst->as_reference.object;
        break;
    }
    case REFERENCE_KIND_NAMED:
        dst_slot = reference_unwind(dst);
        break;
    }

    src_obj = object_deref_if_internal(src);

    if (!overwrite) {
        overwrite = (*dst_slot)->type == src_obj->type ||
                    (*dst_slot)->type == UACPI_OBJECT_UNINITIALIZED;
    }

    if (overwrite)
        return object_overwrite_try_elide(*dst_slot, src_obj);

    return object_assign_with_implicit_cast(*dst_slot, src_obj);
}

static uacpi_status dispatch_1_arg_with_target(struct execution_context *ctx)
{
    uacpi_status ret;
    struct pending_op *pop = ctx->cur_pop;
    uacpi_object *arg0, *tgt, *ret_tgt;

    arg0 = *operand_array_at(&pop->operands, 0);
    tgt = *operand_array_at(&pop->operands, 1);

    ret = exec_get_ret_target(ctx, &ret_tgt);
    if (uacpi_unlikely_error(ret))
        return ret;

    // Someone wants the return value, ref it so that it's not moved into tgt
    if (ret_tgt)
        uacpi_object_ref(arg0);

    switch (pop->code) {
    case UACPI_AML_OP_StoreOp:
    case UACPI_AML_OP_CopyObjectOp: {
        if (tgt->type == UACPI_OBJECT_SPECIAL) {
            ret = special_store(tgt, arg0);
            break;
        }

        if (tgt->type != UACPI_OBJECT_REFERENCE ||
            tgt->common.flags == REFERENCE_KIND_REFOF) {
            uacpi_kernel_log(UACPI_LOG_WARN, "Target is not a SuperName\n");
            ret = UACPI_STATUS_BAD_BYTECODE;
            break;
        }

        if (pop->code == UACPI_AML_OP_StoreOp)
            ret = store_to_reference(tgt, arg0);
        else
            ret = copy_object_to_reference(tgt, arg0);
        break;
    }
    default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
        break;
    }

    if (uacpi_unlikely_error(ret))
        return ret;

    if (ret_tgt) {
        uacpi_object_unref(arg0);
        return object_overwrite_try_elide(ret_tgt, arg0);
    }

    return UACPI_STATUS_OK;
}

static uacpi_status dispatch_0_arg_with_target(struct execution_context *ctx)
{
    uacpi_status ret;
    struct pending_op *pop = ctx->cur_pop;
    uacpi_object *tgt, *res, *ret_tgt;
    uacpi_bool unref_res = UACPI_FALSE;

    tgt = *operand_array_at(&pop->operands, 0);
    if (tgt->type != UACPI_OBJECT_REFERENCE)
        return UACPI_STATUS_BAD_BYTECODE;

    switch (pop->code) {
    case UACPI_AML_OP_IncrementOp:
    case UACPI_AML_OP_DecrementOp: {
        uacpi_i32 val = pop->code == UACPI_AML_OP_IncrementOp ? +1 : -1;

        res = *object_deref_implicit(tgt);
        if (res->type != UACPI_OBJECT_INTEGER)
            return UACPI_STATUS_BAD_BYTECODE;

        res->as_integer.value += val;
        truncate_number_if_needed(res);
        break;
    }
    case UACPI_AML_OP_RefOfOp:
        res = uacpi_create_object(UACPI_OBJECT_REFERENCE);
        if (uacpi_unlikely(res == UACPI_NULL))
            return UACPI_STATUS_OUT_OF_MEMORY;

        res->as_reference.object = object_deref_if_internal(tgt);
        uacpi_object_ref(res->as_reference.object);
        unref_res = UACPI_TRUE;
        break;
    case UACPI_AML_OP_DeRefOfOp:
        ret = object_deref_explicit(tgt, &res);
        if (uacpi_unlikely_error(ret))
            return ret;
        break;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }

    ret = exec_get_ret_target(ctx, &ret_tgt);
    if (uacpi_likely_success(ret) && ret_tgt)
        ret = object_overwrite_try_elide(ret_tgt, res);

    if (unref_res)
        uacpi_object_unref(res);

    return ret;
}

static void do_binary_math(uacpi_object *arg0, uacpi_object *arg1,
                           uacpi_object *ret, uacpi_aml_op op)
{
    uacpi_u64 lhs, rhs, res;
    uacpi_bool should_negate = UACPI_FALSE;

    lhs = arg0->as_integer.value;
    rhs = arg1->as_integer.value;

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
    case UACPI_AML_OP_ModOp:
        res = lhs % rhs;
        break;
    default:
        break;
    }

    if (should_negate)
        res = ~res;

    ret->as_integer.value = res;
    truncate_number_if_needed(ret);
}

static uacpi_status dispatch_3_arg_with_target(struct execution_context *ctx)
{
    uacpi_status ret;
    struct pending_op *pop = ctx->cur_pop;
    uacpi_object *arg0, *arg1, *tgt, *temp_result, *ret_tgt;

    arg0 = *operand_array_at(&pop->operands, 0);
    arg1 = *operand_array_at(&pop->operands, 1);
    tgt = *operand_array_at(&pop->operands, 2);

    temp_result = uacpi_create_object(UACPI_OBJECT_UNINITIALIZED);
    if (uacpi_unlikely(temp_result == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    switch (pop->code) {
    case UACPI_AML_OP_AddOp:
    case UACPI_AML_OP_SubtractOp:
    case UACPI_AML_OP_MultiplyOp:
    case UACPI_AML_OP_ShiftLeftOp:
    case UACPI_AML_OP_ShiftRightOp:
    case UACPI_AML_OP_NandOp:
    case UACPI_AML_OP_AndOp:
    case UACPI_AML_OP_NorOp:
    case UACPI_AML_OP_OrOp:
    case UACPI_AML_OP_XorOp:
    case UACPI_AML_OP_ModOp:
        arg0 = object_deref_if_internal(arg0);
        arg1 = object_deref_if_internal(arg1);

        if (arg0->type != UACPI_OBJECT_INTEGER ||
            arg1->type != UACPI_OBJECT_INTEGER) {
            return UACPI_STATUS_BAD_BYTECODE;
        }
        temp_result->type = UACPI_OBJECT_INTEGER;

        do_binary_math(arg0, arg1, temp_result, pop->code);
        break;
    default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
        break;
    }

    switch (tgt->type) {
    case UACPI_OBJECT_SPECIAL:
        ret = special_store(tgt, temp_result);
        break;
    case UACPI_OBJECT_REFERENCE:
        ret = store_to_reference(tgt, temp_result);
        break;
    case UACPI_OBJECT_INTEGER:
        // NULL target
        if (tgt->as_integer.value == 0)
            break;
    default:
        ret = UACPI_STATUS_BAD_BYTECODE;
    }

    if (uacpi_likely_success(ret)) {
        ret = exec_get_ret_target(ctx, &ret_tgt);
        if (uacpi_likely_success(ret) && ret_tgt)
            ret = object_overwrite_try_elide(ret_tgt, temp_result);
    }

    uacpi_object_unref(temp_result);
    return ret;
}

static uacpi_status dispatch_1_arg(struct execution_context *ctx)
{
    uacpi_status ret;
    struct pending_op *pop = ctx->cur_pop;
    uacpi_object *arg, *ret_tgt;

    arg = *operand_array_at(&pop->operands, 0);
    arg = object_deref_if_internal(arg);

    ret = exec_get_ret_target(ctx, &ret_tgt);
    if (uacpi_unlikely_error(ret))
        return ret;

    switch (pop->code) {
    case UACPI_AML_OP_LnotOp:
        if (arg->type != UACPI_OBJECT_INTEGER)
            return UACPI_STATUS_BAD_BYTECODE;

        if (ret_tgt) {
            ret_tgt->type = UACPI_OBJECT_INTEGER;
            ret_tgt->as_integer.value = arg->as_integer.value ? 0 : ones();
        }
        break;
    default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
        break;
    }

    return ret;
}

static uacpi_status dispatch_2_arg(struct execution_context *ctx)
{
    uacpi_status ret;
    struct pending_op *pop = ctx->cur_pop;
    uacpi_object *arg0, *arg1, *ret_tgt;

    arg0 = *operand_array_at(&pop->operands, 0);
    arg1 = *operand_array_at(&pop->operands, 1);

    arg0 = object_deref_if_internal(arg0);
    arg1 = object_deref_if_internal(arg1);

    ret = exec_get_ret_target(ctx, &ret_tgt);
    if (uacpi_unlikely_error(ret))
        return ret;

    switch (pop->code) {
    case UACPI_AML_OP_LEqualOp: {
        uacpi_bool result;

        if (arg0->type != arg1->type)
            return UACPI_STATUS_BAD_BYTECODE;
        if (!ret_tgt)
            return UACPI_STATUS_OK;

        switch (arg0->type) {
        case UACPI_OBJECT_INTEGER:
            result = arg0->as_integer.value == arg1->as_integer.value;
            break;
        case UACPI_OBJECT_STRING:
        case UACPI_OBJECT_BUFFER:
            result = arg0->as_buffer.size == arg1->as_buffer.size;
            if (result) {
                result = uacpi_memcmp(
                    arg0->as_buffer.data,
                    arg1->as_buffer.data,
                    arg0->as_buffer.size
                ) == 0;
            }
            break;
        default:
            return UACPI_STATUS_BAD_BYTECODE;
        }

        ret_tgt->type = UACPI_OBJECT_INTEGER;
        ret_tgt->as_integer.value = result ? ones() : 0;
        break;
    }
    default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
        break;
    }

    return ret;
}

static uacpi_status exec_dispatch(struct execution_context *ctx)
{
    uacpi_status st = UACPI_STATUS_UNIMPLEMENTED;
    struct uacpi_opcode_exec *op = &ctx->cur_pop->info.as_exec;

    switch (op->operand_count) {
    case 1:
        if (op->has_target)
            st = dispatch_0_arg_with_target(ctx);
        else
            st = dispatch_1_arg(ctx);
        break;
    case 2:
        if (op->has_target)
            st = dispatch_1_arg_with_target(ctx);
        else
            st = dispatch_2_arg(ctx);
        break;
    case 3:
        if (op->has_target)
            st = dispatch_3_arg_with_target(ctx);
    default:
        break;
    }

    return st;
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
                                         uacpi_u32 *out_size)
{
    uacpi_u32 left;
    uacpi_u8 want_bytes = 1;
    uacpi_u8 *data;

    left = call_frame_code_bytes_left(frame);
    if (uacpi_unlikely(left < 1))
        return UACPI_STATUS_BAD_BYTECODE;

    data = call_frame_cursor(frame);
    want_bytes += *data >> 6;

    if (uacpi_unlikely(left < want_bytes))
        return UACPI_STATUS_BAD_BYTECODE;

    switch (want_bytes) {
    case 1:
        *out_size = *data & 0b111111;
        break;
    case 2:
    case 3:
    case 4: {
        uacpi_u32 temp_byte = 0;

        *out_size = *data & 0b1111;
        uacpi_memcpy(&temp_byte, data + 1, want_bytes - 1);

        // want_bytes - 1 is at most 3, so this shift is safe
        *out_size |= temp_byte << 4;
        break;
    }
    }

    frame->code_offset += want_bytes;
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
uacpi_status parse_method_flags(struct call_frame *frame, uacpi_control_method *method)
{
    uacpi_u8 flags_byte;

    if (!call_frame_has_code(frame))
        return UACPI_STATUS_BAD_BYTECODE;

    flags_byte = *(uacpi_u8*)call_frame_cursor(frame);
    method->args = flags_byte & 0b111;
    method->is_serialized = (flags_byte >> 3) & 1;
    method->sync_level = flags_byte >> 4;

    frame->code_offset++;
    return UACPI_STATUS_OK;
}

static uacpi_status create_method(struct call_frame *frame)
{
    uacpi_status ret;
    struct uacpi_control_method *method = UACPI_NULL;
    struct uacpi_namespace_node *node = UACPI_NULL;

    uacpi_object_name name;
    uacpi_size base_offset;

    base_offset = ++frame->code_offset;

    method = uacpi_kernel_alloc(sizeof(*method));
    if (uacpi_unlikely(method == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    ret = parse_package_length(frame, &method->size);
    if (uacpi_unlikely_error(ret))
        goto err_out;

    ret = parse_name(frame, &name);
    if (uacpi_unlikely_error(ret))
        goto err_out;

    node = uacpi_namespace_node_alloc(name);
    if (uacpi_unlikely(node == UACPI_NULL)) {
        ret = UACPI_STATUS_OUT_OF_MEMORY;
        goto err_out;
    }

    ret = parse_method_flags(frame, method);
    if (uacpi_unlikely_error(ret))
        goto err_out;

    method->code = call_frame_cursor(frame);
    method->size -= frame->code_offset - base_offset;
    frame->code_offset += method->size;

    node->object.type = UACPI_OBJECT_METHOD;
    node->object.as_method.method = method;

    ret = uacpi_node_install(UACPI_NULL, node);
    if (uacpi_unlikely_error(ret))
        goto err_out;

    ret = UACPI_STATUS_OK;
    return ret;

err_out:
    uacpi_kernel_free(method);
    uacpi_namespace_node_free(node);
    return ret;
}

static uacpi_status create_dispatch(struct call_frame *frame)
{
    switch (frame->cur_op.code) {
    case UACPI_AML_OP_MethodOp:
        return create_method(frame);
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

static uacpi_status pop_prime(struct execution_context *ctx)
{
    struct pending_op *pop;
    struct call_frame *frame = ctx->cur_frame;

    pop = pending_op_array_calloc(&frame->pending_ops);
    if (uacpi_unlikely(pop == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    pop->code = frame->cur_op.code;
    pop->info = frame->cur_op.info;
    ctx->cur_pop = pop;

    return UACPI_STATUS_OK;
}

static uacpi_status method_call_init(struct execution_context *ctx)
{
    return pop_prime(ctx);
}

static uacpi_status flow_init(struct execution_context *ctx)
{
    struct call_frame *frame = ctx->cur_frame;

    if (pending_op_array_size(&ctx->cur_frame->pending_ops) != 0)
        return UACPI_STATUS_BAD_BYTECODE;

    switch (frame->cur_op.code) {
    case UACPI_AML_OP_ContinueOp:
    case UACPI_AML_OP_BreakOp:
        if (frame->last_while == UACPI_NULL)
            return UACPI_STATUS_BAD_BYTECODE;
    case UACPI_AML_OP_ReturnOp:
        call_frame_advance_pc(frame);
        break;
    case UACPI_AML_OP_IfOp:
    case UACPI_AML_OP_ElseOp:
    case UACPI_AML_OP_WhileOp: {
        struct uacpi_opcode_flow *flow;
        uacpi_status st;
        uacpi_u32 len;

        flow = &frame->cur_op.info.as_flow;
        flow->start_offset = ctx->cur_frame->code_offset++;

        st = parse_package_length(ctx->cur_frame, &len);
        if (uacpi_unlikely_error(st))
            return st;

        // +1 because size of the op is not included in the package length
        flow->end_offset = flow->start_offset + len + 1;
        if (uacpi_unlikely(flow->end_offset < flow->start_offset))
            return UACPI_STATUS_BAD_BYTECODE;

        if (frame->cur_op.code == UACPI_AML_OP_ElseOp && ctx->skip_else) {
            uacpi_kernel_log(UACPI_LOG_TRACE,
                             "Else skipped because an If was taken earlier\n");
            frame->code_offset = flow->end_offset;
            return UACPI_STATUS_OK;
        }
        break;
    }
    default:
        break;
    }

    return pop_prime(ctx);
}

static uacpi_status exec_init(struct execution_context *ctx)
{
    call_frame_advance_pc(ctx->cur_frame);
    return pop_prime(ctx);
}

static void execution_context_release(struct execution_context *ctx)
{
    if (ctx->ret)
        uacpi_object_unref(ctx->ret);
    call_frame_array_clear(&ctx->call_stack);
    uacpi_kernel_free(ctx);
}

static uacpi_status get_arg(struct call_frame *frame)
{
    uacpi_status ret = UACPI_STATUS_UNIMPLEMENTED;
    struct uacpi_opcode_arg *op = &frame->cur_op.info.as_arg;

    switch (op->arg_type) {
    case UACPI_ARG_TYPE_ANY:
        switch (op->sub_type) {
        case UACPI_ARG_SUB_TYPE_LOCAL:
        case UACPI_ARG_SUB_TYPE_ARG:
            ret = get_arg_or_local_ref(frame, op->sub_type);
            break;
        default:
            break;
        }
        break;
    case UACPI_ARG_TYPE_NUMBER:
        ret = get_number(frame);
        break;
    case UACPI_ARG_TYPE_STRING:
        ret = get_string(frame);
        break;
    case UACPI_ARG_TYPE_SPECIAL:
        ret = get_special(frame);
        break;
    default:
        break;
    }

    return ret;
}

static void operand_array_release(struct operand_array *operands)
{
    uacpi_size i;

    for (i = 0; i < operand_array_size(operands); ++i)
        uacpi_object_unref(*operand_array_at(operands, i));

    operand_array_clear(operands);
}

static void call_frame_clear(struct call_frame *frame)
{
    uacpi_size i;

    pending_op_array_clear(&frame->pending_ops);
    flow_frame_array_clear(&frame->flows);

    for (i = 0; i < 7; ++i)
        uacpi_object_unref(frame->args[i]);
    for (i = 0; i < 8; ++i)
        uacpi_object_unref(frame->locals[i]);
}

static void ctx_reload_post_dispatch(struct execution_context *ctx)
{
    operand_array_release(&ctx->cur_pop->operands);
    pending_op_array_pop(&ctx->cur_frame->pending_ops);

    ctx->cur_pop = pending_op_array_last(&ctx->cur_frame->pending_ops);
}

static void ctx_reload_post_ret(struct execution_context* ctx)
{
    if (ctx->cur_pop)
        operand_array_release(&ctx->cur_pop->operands);

    call_frame_clear(ctx->cur_frame);
    call_frame_array_pop(&ctx->call_stack);

    ctx->cur_frame = call_frame_array_last(&ctx->call_stack);
    if (ctx->cur_frame) {
        ctx->cur_pop = pending_op_array_last(&ctx->cur_frame->pending_ops);
        ctx->cur_flow = flow_frame_array_last(&ctx->cur_frame->flows);
    } else {
        ctx->cur_pop = UACPI_NULL;
        ctx->cur_flow = UACPI_NULL;
    }
}

#define UACPI_OP_TRACING

static void trace_op(struct op *op)
{
#ifdef UACPI_OP_TRACING
    if (op->code == UACPI_AML_OP_UACPIInternalOpMethodCall) {
        uacpi_char buf[5];

        buf[4] = '\0';
        uacpi_memcpy(buf, op->info.as_method_call.node->name.text, 4);
        uacpi_kernel_log(UACPI_LOG_TRACE, "Processing MethodCall to '%s'\n",
                         buf);
        return;
    }

    uacpi_kernel_log(UACPI_LOG_TRACE, "Processing Op '%s'\n",
                     op->info.name);
#endif
}

static void frame_push_args(struct call_frame *frame,
                            struct pending_op *invocation)
{
    uacpi_size i;

    for (i = 0; i < operand_array_size(&invocation->operands); ++i) {
        uacpi_object *obj = *operand_array_at(&invocation->operands, i);
        frame->args[i] = obj;

        /*
         * If argument is a LocalX or ArgX and the referenced type is an
         * integer then we just copy the object
         */
        if (obj->type != UACPI_OBJECT_REFERENCE)
            goto next;
        if (obj->common.flags != REFERENCE_KIND_LOCAL &&
            obj->common.flags != REFERENCE_KIND_ARG)
            goto next;

        obj = object_deref_if_internal(obj);
        if (obj->type != UACPI_OBJECT_INTEGER)
            goto next;

        object_overwrite_try_elide(frame->args[i], obj);

    next:
        uacpi_object_ref(frame->args[i]);
    }
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
    ctx->cur_pop = pending_op_array_last(&prev_frame->pending_ops);
    ctx->cur_flow = flow_frame_array_last(&prev_frame->flows);

    return UACPI_STATUS_OK;
}

static uacpi_status method_call_dispatch(struct execution_context *ctx)
{
    uacpi_status ret;

    struct uacpi_opcode_info *info = &ctx->cur_pop->info;
    struct uacpi_namespace_node *node = info->as_method_call.node;
    struct uacpi_control_method *method = node->object.as_method.method;
    struct call_frame *frame;

    if (uacpi_unlikely(operand_array_size(&ctx->cur_pop->operands)
                       != method->args))
        return UACPI_STATUS_BAD_BYTECODE;

    ret = push_new_frame(ctx, &frame);
    if (uacpi_unlikely_error(ret))
        return ret;

    frame_push_args(frame, ctx->cur_pop);
    ctx_reload_post_dispatch(ctx);

    ctx->cur_frame = frame;
    ctx->cur_frame->method = method;
    ctx->cur_pop = UACPI_NULL;

    return UACPI_STATUS_OK;
}

static uacpi_status maybe_dispatch_op(struct execution_context *ctx)
{
    uacpi_status ret = UACPI_STATUS_OK;
    struct pending_op *pop;

    for (;;) {
        pop = ctx->cur_pop;

        if (!pop || !op_dispatchable(pop))
            break;

        switch (pop->info.type) {
        case UACPI_OPCODE_TYPE_FLOW:
            ret = flow_dispatch(ctx);
            break;
        case UACPI_OPCODE_TYPE_EXEC:
            ret = exec_dispatch(ctx);
            break;
        case UACPI_OPCODE_TYPE_METHOD_CALL:
            return method_call_dispatch(ctx);
        default:
            ret = UACPI_STATUS_UNIMPLEMENTED;
        }

        if (ret != UACPI_STATUS_OK)
            break;

        ctx_reload_post_dispatch(ctx);
    }

    return ret;
}

static uacpi_bool maybe_end_flow(struct execution_context *ctx)
{
    struct flow_frame *flow = ctx->cur_flow;
    struct call_frame *cur_frame = ctx->cur_frame;
    uacpi_bool ret = UACPI_FALSE;

    if (!flow)
        return ret;
    if (cur_frame->code_offset != flow->end)
        return ret;

    ctx->skip_else = UACPI_FALSE;

    if (flow->type == FLOW_FRAME_WHILE) {
        cur_frame->code_offset = flow->begin;
    } else if (flow->type == FLOW_FRAME_IF) {
        ctx->skip_else = UACPI_TRUE;
        ret = UACPI_TRUE;
    }

    frame_reset_post_end_flow(ctx, flow->type == FLOW_FRAME_WHILE);
    return ret;
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

    for (;;) {
        if (uacpi_unlikely_error(st))
            break;
        if (ctx->cur_frame == UACPI_NULL)
            break;

        st = maybe_dispatch_op(ctx);
        if (uacpi_unlikely_error(st))
            break;

        if (maybe_end_flow(ctx))
            continue;

        cur_frame = ctx->cur_frame;
        if (!call_frame_has_code(cur_frame)) {
            ctx_reload_post_ret(ctx);
            continue;
        }

        cur_op = &cur_frame->cur_op;

        st = peek_op(cur_frame);
        if (uacpi_unlikely_error(st))
            goto out;

        trace_op(cur_op);

        switch (cur_op->info.type)
        {
        case UACPI_OPCODE_TYPE_EXEC:
            st = exec_init(ctx);
            break;
        case UACPI_OPCODE_TYPE_METHOD_CALL:
            st = method_call_init(ctx);
            break;
        case UACPI_OPCODE_TYPE_FLOW:
            st = flow_init(ctx);
            break;
        case UACPI_OPCODE_TYPE_ARG:
            call_frame_advance_pc(cur_frame);
            st = get_arg(cur_frame);
            break;
        case UACPI_OPCODE_TYPE_CREATE:
            st = create_dispatch(cur_frame);
            break;
        default:
            uacpi_kernel_log(UACPI_LOG_WARN, "Unimplemented opcode type %u\n",
                             cur_op->info.type);
            st = UACPI_STATUS_UNIMPLEMENTED;
            goto out;
        }
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
