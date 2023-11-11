#include <uacpi/internal/interpreter.h>
#include <uacpi/internal/dynamic_array.h>
#include <uacpi/internal/opcodes.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/platform/stdlib.h>
#include <uacpi/kernel_api.h>

enum operand_type {
    OPERAND_TYPE_DEFAULT,

    /*
     * Rebindable reference, stores change the object itself.
     */
    OPERAND_TYPE_LOCAL_REF,

    /*
     * Non-rebindable reference, stores go directly
     * into the referenced object.
     */
    OPERAND_TYPE_ARG_REF,

    /*
     * Reference to a named object, stores have to go
     * through implicit conversion unless CopyObject is used.
     */
    OPERAND_TYPE_NAMED_REF,
};

struct operand {
    enum operand_type type;
    uacpi_object *obj;
};

DYNAMIC_ARRAY_WITH_INLINE_STORAGE(operand_array, struct operand, 8)
DYNAMIC_ARRAY_WITH_INLINE_STORAGE_IMPL(operand_array, struct operand, static)

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

    struct operand args[7];
    struct operand locals[8];

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

uacpi_status pop_operand_alloc(struct pending_op *pop, struct operand **out_operand)
{
    struct operand *operand;

    operand = operand_array_calloc(&pop->operands);
    if (operand == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    operand->obj = uacpi_create_object(UACPI_OBJECT_NULL);
    if (operand->obj == UACPI_NULL) {
        operand_array_pop(&pop->operands);
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    *out_operand = operand;
    return UACPI_STATUS_OK;
}

uacpi_status next_arg(struct call_frame *frame, struct operand **out_operand)
{
    struct pending_op *pop;

    pop = pending_op_array_last(&frame->pending_ops);
    // Just a stray argument in the bytecode
    if (pop == UACPI_NULL)
        return UACPI_STATUS_BAD_BYTECODE;

    return pop_operand_alloc(pop, out_operand);
}

static uacpi_status next_arg_unwrapped(struct call_frame *frame,
                                       uacpi_object **out_obj)
{
    uacpi_status ret;
    struct operand *operand;

    ret = next_arg(frame, &operand);
    if (uacpi_unlikely_error(ret))
        return ret;

    *out_obj = operand->obj;
    return UACPI_STATUS_OK;
}

uacpi_status get_string(struct call_frame *frame)
{
    uacpi_status ret;
    uacpi_object *obj;
    char *string;
    size_t length;

    ret = next_arg_unwrapped(frame, &obj);
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

static uacpi_status copy_object_try_elide(uacpi_object *dst, uacpi_object *src)
{
    uacpi_status ret = UACPI_STATUS_OK;

    switch (src->type) {
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
        uacpi_object_reference *ref = &src->as_reference;
        ref->object = src->as_reference.object;
        uacpi_object_ref(ref->object);
        break;
    } default:
        ret = UACPI_STATUS_UNIMPLEMENTED;
    }

    if (ret == UACPI_STATUS_OK)
        dst->type = src->type;

    return ret;
}

static uacpi_object *object_unwrap(uacpi_object *object)
{
    if (object->type != UACPI_OBJECT_REFERENCE)
        return object;

    return object->as_reference.object;
}

static uacpi_object *operand_unwrap(struct operand *operand,
                                    uacpi_bool only_if_internal_ref)
{
    if (operand->type == OPERAND_TYPE_LOCAL_REF ||
        operand->type == OPERAND_TYPE_ARG_REF)
        goto unwrap_ref;
    if (only_if_internal_ref)
        return operand->obj;

unwrap_ref:
    return object_unwrap(operand->obj);
}

static uacpi_status copy_retval(uacpi_object *dst, struct operand *src)
{
    return copy_object_try_elide(dst, operand_unwrap(src, UACPI_TRUE));
}

static uacpi_status get_arg_or_local_ref(
    struct call_frame *frame, enum uacpi_arg_sub_type sub_type
)
{
    uacpi_status ret;
    struct operand *src, *dst;
    enum operand_type ref_type;

    switch (sub_type) {
    case UACPI_ARG_SUB_TYPE_LOCAL:
        src = &frame->locals[frame->cur_op.code - UACPI_AML_OP_Local0Op];
        ref_type = OPERAND_TYPE_LOCAL_REF;
        break;
    case UACPI_ARG_SUB_TYPE_ARG:
        src = &frame->args[frame->cur_op.code - UACPI_AML_OP_Arg0Op];
        ref_type = OPERAND_TYPE_ARG_REF;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    ret = next_arg(frame, &dst);
    if (uacpi_unlikely_error(ret))
        return ret;

    // Access to an uninitialized local or arg, hopefully a store incoming
    if (src->obj == UACPI_NULL) {
        src->obj = uacpi_create_object(UACPI_OBJECT_NULL);
        if (uacpi_unlikely(src->obj == UACPI_NULL))
            return UACPI_STATUS_OUT_OF_MEMORY;
    }

    dst->type = ref_type;
    dst->obj->type = UACPI_OBJECT_REFERENCE;
    dst->obj->as_reference.object = src->obj;
    uacpi_object_ref(src->obj);

    return ret;
}

uacpi_status get_number(struct call_frame *frame)
{
    uacpi_status ret;
    uacpi_object *obj;
    void *data;
    uacpi_u8 bytes;

    ret = next_arg_unwrapped(frame, &obj);
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
    frame->code_offset += bytes;

out:
    obj->type = UACPI_OBJECT_INTEGER;
    return UACPI_STATUS_OK;
}

uacpi_status get_special(struct call_frame *frame)
{
    uacpi_status ret;
    uacpi_object *obj;

    ret = next_arg_unwrapped(frame, &obj);
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
                                          struct operand **out_operand)
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
                                        struct operand **out_operand)
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

    return UACPI_STATUS_NOT_FOUND;
}

static uacpi_status method_get_ret_object(struct execution_context *ctx,
                                          uacpi_object **out_obj)
{
    struct operand *operand;
    uacpi_status ret;

    ret = method_get_ret_target(ctx, &operand);
    if (ret == UACPI_STATUS_NOT_FOUND) {
        *out_obj = ctx->ret;
        return UACPI_STATUS_OK;
    }
    if (ret != UACPI_STATUS_OK || operand == UACPI_NULL)
        return ret;

    *out_obj = operand_unwrap(operand, UACPI_TRUE);
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

static uacpi_status predicate_evaluate(struct operand *operand, uacpi_bool *res)
{
    uacpi_object *unwrapped_obj;

    unwrapped_obj = operand_unwrap(operand, UACPI_TRUE);
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

        return copy_retval(dst, operand_array_at(&pop->operands, 0));
    }
    case UACPI_AML_OP_ElseOp:
        return begin_flow_execution(ctx);
    case UACPI_AML_OP_IfOp:
    case UACPI_AML_OP_WhileOp: {
        uacpi_bool res;

        ret = predicate_evaluate(operand_array_at(&pop->operands, 0), &res);
        if (uacpi_unlikely_error(ret))
            return ret;

        return handle_predicate_result(ctx, res);
    }
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

static uacpi_status special_store(struct operand *dst, struct operand *src)
{
    uacpi_object *unwrapped_src;

    if (dst->obj->as_special.special_type != UACPI_SPECIAL_TYPE_DEBUG_OBJECT)
        return UACPI_STATUS_INVALID_ARGUMENT;

    unwrapped_src = operand_unwrap(src, UACPI_TRUE);

    switch (unwrapped_src->type) {
    case UACPI_OBJECT_STRING:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, String] %s\n",
                         unwrapped_src->as_string.text);
        break;
    case UACPI_OBJECT_INTEGER:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Integer] 0x%016llX\n",
                         unwrapped_src->as_integer.value);
        break;
    case UACPI_OBJECT_REFERENCE:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Reference] Object @%p\n",
                         unwrapped_src);
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }

    return UACPI_STATUS_OK;
}

static uacpi_object **operand_get_target_object(struct operand *dst)
{
    switch (dst->type) {
    case OPERAND_TYPE_DEFAULT:
    case OPERAND_TYPE_LOCAL_REF:
        if (dst->obj->type == UACPI_OBJECT_REFERENCE)
            return &dst->obj->as_reference.object;

        return &dst->obj;
    case OPERAND_TYPE_ARG_REF: {
        uacpi_object **dst_obj;

        dst_obj = &dst->obj->as_reference.object;
        if ((*dst_obj)->type != UACPI_OBJECT_REFERENCE)
            return dst_obj;

        return &((*dst_obj)->as_reference.object);
    } default:
        return UACPI_NULL;
    }
}

static uacpi_status reference_store(struct operand *dst, struct operand *src)
{
    uacpi_object **dst_obj;
    uacpi_object **src_obj;

    dst_obj = operand_get_target_object(dst);
    if (uacpi_unlikely(dst_obj == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;

    src_obj = operand_get_target_object(src);
    if (uacpi_unlikely(dst_obj == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;

    copy_object_try_elide(*dst_obj, *src_obj);
    return UACPI_STATUS_OK;
}

static uacpi_status operand_store(struct operand *dst, uacpi_object *src)
{
    uacpi_object **dst_obj;

    dst_obj = operand_get_target_object(dst);
    if (uacpi_unlikely(dst_obj == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;

    uacpi_object_unref(*dst_obj);
    *dst_obj = src;
    uacpi_object_ref(src);
    return UACPI_STATUS_OK;
}

static uacpi_status result_store(struct execution_context *ctx,
                                 struct operand *res)
{
    uacpi_status ret;
    struct operand *ret_tgt = UACPI_NULL;

    ret = exec_get_ret_target(ctx, &ret_tgt);
    if (ret == UACPI_STATUS_NOT_FOUND)
        ret = UACPI_STATUS_OK;
    if (ret != UACPI_STATUS_OK)
        return ret;

    if (ret_tgt) {
        ret_tgt->type = res->type;
        return operand_store(ret_tgt, res->obj);
    }

    return UACPI_STATUS_OK;
}

static uacpi_status dispatch_1_arg_with_target(struct execution_context *ctx)
{
    uacpi_status ret = UACPI_STATUS_UNIMPLEMENTED;
    struct pending_op *pop = ctx->cur_pop;
    struct operand *arg0, *tgt;

    arg0 = operand_array_at(&pop->operands, 0);
    tgt = operand_array_at(&pop->operands, 1);

    switch (pop->code) {
    case UACPI_AML_OP_StoreOp: {
        switch (tgt->obj->type) {
        case UACPI_OBJECT_SPECIAL:
            ret = special_store(tgt, arg0);
            break;
        case UACPI_OBJECT_REFERENCE:
            ret = reference_store(tgt, arg0);
            break;
        default:
            // TODO: expand on why it's broken
            uacpi_kernel_log(UACPI_LOG_WARN, "Broken store(?)");
            break;
        }
        break;
    default:
        break;
    }
    }

    if (uacpi_unlikely_error(ret))
        return ret;

    return result_store(ctx, tgt);
}

static uacpi_status dispatch_0_arg_with_target(struct execution_context *ctx)
{
    struct pending_op *pop = ctx->cur_pop;
    struct operand *tgt;

    tgt = operand_array_at(&pop->operands, 0);

    switch (pop->code) {
    case UACPI_AML_OP_IncrementOp:
    case UACPI_AML_OP_DecrementOp: {
        uacpi_object *obj;
        uacpi_i32 val = pop->code == UACPI_AML_OP_IncrementOp ? +1 : -1;

        if (tgt->obj->type != UACPI_OBJECT_REFERENCE)
            return UACPI_STATUS_BAD_BYTECODE;

        obj = *operand_get_target_object(tgt);
        if (obj->type != UACPI_OBJECT_INTEGER)
            return UACPI_STATUS_BAD_BYTECODE;

        obj->as_integer.value += val;
        return result_store(
            ctx,
            &(struct operand) { .type = OPERAND_TYPE_DEFAULT, obj }
        );
    }
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

static uacpi_status exec_dispatch(struct execution_context *ctx)
{
    uacpi_status st = UACPI_STATUS_UNIMPLEMENTED;
    struct uacpi_opcode_exec *op = &ctx->cur_pop->info.as_exec;

    switch (op->operand_count) {
    case 1:
        if (op->has_target)
            st = dispatch_0_arg_with_target(ctx);
        break;
    case 2:
        if (op->has_target)
            st = dispatch_1_arg_with_target(ctx);
        break;
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

    for (i = 0; i < operand_array_size(operands); ++i) {
        struct operand *operand;

        operand = operand_array_at(operands, i);
        uacpi_object_unref(operand->obj);
    }

    operand_array_clear(operands);
}

static void call_frame_clear(struct call_frame *frame)
{
    uacpi_size i;

    pending_op_array_clear(&frame->pending_ops);
    flow_frame_array_clear(&frame->flows);

    for (i = 0; i < 7; ++i)
        uacpi_object_unref(frame->args[i].obj);
    for (i = 0; i < 8; ++i)
        uacpi_object_unref(frame->locals[i].obj);
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
        frame->args[i].obj = operand_unwrap(
            operand_array_at(&invocation->operands, i),
            UACPI_TRUE
        );
        uacpi_object_ref(frame->args[i].obj);
    }
}

static uacpi_status method_call_dispatch(struct execution_context *ctx)
{
    struct uacpi_opcode_info *info = &ctx->cur_pop->info;
    struct uacpi_namespace_node *node = info->as_method_call.node;
    struct uacpi_control_method *method = node->object.as_method.method;
    struct call_frame *frame;

    if (uacpi_unlikely(operand_array_size(&ctx->cur_pop->operands)
                       != method->args))
        return UACPI_STATUS_BAD_BYTECODE;

    frame = call_frame_array_calloc(&ctx->call_stack);
    if (frame == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

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
        ctx->ret = uacpi_create_object(UACPI_OBJECT_NULL);
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
            ctx->cur_frame->args[i].obj = args->objects[i];
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
    if (ret && ctx->ret->type != UACPI_OBJECT_NULL) {
        uacpi_object_ref(ctx->ret);
        *ret = ctx->ret;
    }
    execution_context_release(ctx);
    return st;
}
