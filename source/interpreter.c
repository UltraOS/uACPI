#include <uacpi/internal/interpreter.h>
#include <uacpi/internal/dynamic_array.h>
#include <uacpi/internal/opcodes.h>
#include <uacpi/internal/namespace.h>
#include <uacpi/platform/stdlib.h>
#include <uacpi/kernel_api.h>

struct operand {
    uacpi_u32 flags;
    uacpi_object obj;
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

struct call_frame {
    struct uacpi_control_method *method;
    struct op cur_op;

    /*
     * Each op with operands gets a 'pending op', e.g. for the following code:
     * ---------------------------------------------------------------
     * Return (GETX(GETY(ADD(5, GETZ()))))
     * ---------------------------------------------------------------
     * The op contexts would look like this:
     * op_ctx[0] = ReturnOp, expected_args = 1, args = <pending>
     * op_ctx[1] = MethodCall (GETX), expected_args = 1, args = <pending>
     * op_ctx[2] = MethodCall (GETY), expected_args = 1, args = <pending>
     * op_ctx[3] = AddOp, expected_args = 2, args[0] = 5, args[1] = <pending>
     * GETZ (currently being executed)
     *
     * The idea is that as soon as a 'pending op' gets its
     * arg_count == target_arg_count it is dispatched (aka executed) right
     * away, in a sort of "tetris" way. This allows us to guarantee left to
     * right execution (same as ACPICA) and also zero stack usage as all of
     * this logic happens within one function.
     */
    struct pending_op_array pending_ops;

    uacpi_size code_offset;
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

struct execution_context {
    struct uacpi_retval *ret;
    struct call_frame_array call_stack;

    struct op cur_op;
    struct call_frame *cur_frame;
    struct uacpi_control_method *cur_method;
    struct pending_op *op_ctx;
    uacpi_bool increment_pc;
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

uacpi_object *next_arg(struct call_frame *frame)
{
    struct pending_op *op;

    op = pending_op_array_last(&frame->pending_ops);
    return &operand_array_calloc(&op->operands)->obj;
}

uacpi_status get_string(struct call_frame *frame)
{
    uacpi_object *obj;
    struct uacpi_control_method *method = frame->method;
    char* string;
    size_t length;

    obj = next_arg(frame);
    string = call_frame_cursor(frame);

    // TODO: sanitize string for valid UTF-8
    length = uacpi_strnlen(string, call_frame_code_bytes_left(frame));

    if (string[length] != 0x00)
        return UACPI_STATUS_BAD_BYTECODE;

    obj->as_string.type = UACPI_OBJECT_STRING;
    obj->as_string.text = uacpi_kernel_alloc(length + 1);
    if (uacpi_unlikely(obj->as_string.text == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    uacpi_memcpy(obj->as_string.text, string, length + 1);
    obj->as_string.length = length;
    frame->code_offset += length + 1;
    return UACPI_STATUS_OK;
}

uacpi_status get_number(struct call_frame *frame)
{
    uacpi_object *obj;
    struct uacpi_control_method *method = frame->method;
    void* data;
    uacpi_u8 bytes;

    obj = next_arg(frame);
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
    uacpi_object *obj;

    obj = next_arg(frame);
    switch (frame->cur_op.code) {
    case UACPI_AML_OP_DebugOp:
        obj->as_special.type = UACPI_OBJECT_SPECIAL;
        obj->as_special.special_type = UACPI_SPECIAL_TYPE_DEBUG_OBJECT;
        return UACPI_STATUS_OK;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
}

static uacpi_object *method_get_ret_target(struct execution_context *ctx)
{
    uacpi_size depth;
    struct call_frame* frame = ctx->cur_frame;

    // Check if we're targeting the previous call frame
    depth = call_frame_array_size(&ctx->call_stack);
    if (depth > 1) {
        struct pending_op* pop;
        frame = call_frame_array_at(&ctx->call_stack, depth - 2);
        depth = pending_op_array_size(&frame->pending_ops);

        // Ok, no one wants the return value at call site. Discard it.
        if (!depth)
            return UACPI_NULL;

        pop = pending_op_array_at(&frame->pending_ops, depth - 1);
        return &operand_array_alloc(&pop->operands)->obj;
    }

    // Outermost call frame, defer to external ret
    return ctx->ret ? &ctx->ret->object : UACPI_NULL;
}

static uacpi_object *exec_get_ret_target(struct execution_context *ctx)
{
    uacpi_size depth;
    struct pending_op_array *pops = &ctx->cur_frame->pending_ops;

    // Check if we have a pending op looking for args
    depth = pending_op_array_size(pops);
    if (depth > 1) {
        struct pending_op *pop = pending_op_array_at(pops, depth - 2);
        return &operand_array_alloc(&pop->operands)->obj;
    }

    return UACPI_NULL;
}

static uacpi_status flow_dispatch(struct execution_context *ctx)
{
    struct call_frame *cur_frame = ctx->cur_frame;
    struct pending_op *pop = ctx->op_ctx;

    switch (pop->code) {
    case UACPI_AML_OP_ReturnOp: {
        uacpi_object *retval;

        cur_frame->code_offset = cur_frame->method->size;

        retval = method_get_ret_target(ctx);
        if (retval)
            *retval = operand_array_at(&pop->operands, 0)->obj;

        return UACPI_STATUS_OK;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    }
}

static uacpi_status special_store(uacpi_object *src, uacpi_object *dst)
{
    if (dst->as_special.special_type != UACPI_SPECIAL_TYPE_DEBUG_OBJECT)
        return UACPI_STATUS_INVALID_ARGUMENT;

    switch (src->type) {
    case UACPI_OBJECT_STRING:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, String] %s\n",
                         src->as_string.text);
        break;
    case UACPI_OBJECT_INTEGER:
        uacpi_kernel_log(UACPI_LOG_INFO, "[AML DEBUG, Integer] 0x%016llX\n",
                         src->as_integer.value);
        break;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }

    return UACPI_STATUS_OK;
}

static uacpi_status dispatch_1_arg_with_target(struct execution_context *ctx)
{
    uacpi_status ret = UACPI_STATUS_UNIMPLEMENTED;
    struct pending_op *pop = ctx->op_ctx;
    uacpi_object *arg0 = &operand_array_at(&pop->operands, 0)->obj;
    uacpi_object *tgt = &operand_array_at(&pop->operands, 1)->obj;

    switch (pop->code) {
    case UACPI_AML_OP_StoreOp: {
        switch (tgt->type) {
        case UACPI_OBJECT_SPECIAL:
            ret = special_store(arg0, tgt);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    }

    if (ret == UACPI_STATUS_OK) {
        uacpi_object *ret_tgt = exec_get_ret_target(ctx);
        if (ret_tgt)
            *ret_tgt = *tgt;
    }

    return ret;
}

static uacpi_status exec_dispatch(struct execution_context *ctx)
{
    uacpi_status st = UACPI_STATUS_UNIMPLEMENTED;
    struct uacpi_opcode_exec *op = &ctx->op_ctx->info.as_exec;

    switch (op->operand_count) {
    case 2:
        if (op->has_target)
            st = dispatch_1_arg_with_target(ctx);
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
                                         uacpi_size *out_size)
{
    uacpi_size left;
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
    uacpi_u8 len_bytes;

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
    struct call_frame* frame = ctx->cur_frame;

    pop = pending_op_array_calloc(&frame->pending_ops);
    if (uacpi_unlikely(pop == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    pop->code = frame->cur_op.code;
    pop->info = frame->cur_op.info;
    ctx->op_ctx = pop;

    return UACPI_STATUS_OK;
}

static uacpi_status method_call_init(struct execution_context *ctx)
{
    ctx->increment_pc = UACPI_FALSE;
    return pop_prime(ctx);
}

static uacpi_status flow_init(struct execution_context *ctx)
{
    if (pending_op_array_size(&ctx->cur_frame->pending_ops) != 0)
        return UACPI_STATUS_BAD_BYTECODE;

    return pop_prime(ctx);
}

static uacpi_status exec_init(struct execution_context *ctx)
{
    return pop_prime(ctx);
}

static void execution_context_release(struct execution_context *ctx)
{
    call_frame_array_clear(&ctx->call_stack);
    uacpi_kernel_free(ctx);
}

static uacpi_size op_size(struct op *op)
{
    if ((op->code >> 8) == UACPI_EXT_PREFIX)
        return 2;

    return 1;
}

static uacpi_status get_arg(struct call_frame *frame)
{
    uacpi_status ret;
    struct uacpi_opcode_arg *op = &frame->cur_op.info.as_arg;

    switch (op->arg_type) {
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
        uacpi_kernel_log(UACPI_LOG_WARN, "Unimplemented arg type %u\n",
                         op->arg_type);
        ret = UACPI_STATUS_UNIMPLEMENTED;
    }

    return ret;
}

static void ctx_reload_post_dispatch(struct execution_context *ctx)
{
    operand_array_clear(&ctx->op_ctx->operands);
    pending_op_array_pop(&ctx->cur_frame->pending_ops);

    ctx->op_ctx = pending_op_array_last(&ctx->cur_frame->pending_ops);
}

static void ctx_reload_post_ret(struct execution_context* ctx)
{
    if (ctx->op_ctx)
        operand_array_clear(&ctx->op_ctx->operands);

    pending_op_array_clear(&ctx->cur_frame->pending_ops);
    call_frame_array_pop(&ctx->call_stack);

    ctx->cur_frame = call_frame_array_last(&ctx->call_stack);
    if (ctx->cur_frame) {
        ctx->op_ctx = pending_op_array_last(&ctx->cur_frame->pending_ops);
    } else {
        ctx->op_ctx = UACPI_NULL;
    }
}

#define UACPI_OP_TRACING

static void trace_op(struct op* op)
{
#ifdef UACPI_OP_TRACING
    if (op->code == UACPI_AML_OP_UACPIInternalOpMethodCall) {
        uacpi_char buf[5];

        buf[4] = '\0';
        uacpi_memcpy(buf, op->info.as_method_call.node->name.text, 4);
        uacpi_kernel_log(UACPI_LOG_TRACE, "Processing MethodCall to %s\n",
                         buf);
        return;
    }

    uacpi_kernel_log(UACPI_LOG_TRACE, "Processing Op %s\n",
                     op->info.name);
#endif
}

static uacpi_status method_call_dispatch(struct execution_context *ctx)
{
    struct uacpi_opcode_info *info = &ctx->op_ctx->info;
    struct uacpi_namespace_node *node = info->as_method_call.node;
    struct uacpi_control_method *method = node->object.as_method.method;

    ctx_reload_post_dispatch(ctx);

    ctx->cur_frame = call_frame_array_calloc(&ctx->call_stack);
    if (ctx->cur_frame == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    ctx->cur_frame->method = method;
    ctx->op_ctx = UACPI_NULL;

    return UACPI_STATUS_OK;
}

static uacpi_status dispatch_op(struct execution_context *ctx)
{
    uacpi_status ret;
    struct pending_op *pop = ctx->op_ctx;

    for (;;) {
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

        // Unwind methods at their tails
        while (ctx->cur_frame && !call_frame_has_code(ctx->cur_frame)) {
            ctx_reload_post_ret(ctx);

            // We might have more ops ready to go
            if (ctx->op_ctx && op_dispatchable(ctx->op_ctx))
                break;
        }

        if (!ctx->op_ctx)
            break;
    }

    return ret;
}

uacpi_status uacpi_execute_control_method(uacpi_control_method *method,
                                          uacpi_args *args, uacpi_retval *ret)
{
    uacpi_status st;
    struct execution_context *ctx;
    struct call_frame *cur_frame;
    struct op *cur_op;

    ctx = uacpi_kernel_calloc(1, sizeof(*ctx));
    if (ctx == UACPI_NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    ctx->ret = ret;
    ctx->cur_method = method;

    ctx->cur_frame = call_frame_array_calloc(&ctx->call_stack);
    if (uacpi_unlikely(ctx->cur_frame == UACPI_NULL)) {
        st = UACPI_STATUS_OUT_OF_MEMORY;
        goto out;
    }
    ctx->cur_frame->method = ctx->cur_method;

    for (;;) {
        if (ctx->cur_frame == UACPI_NULL)
            goto out;

        cur_frame = ctx->cur_frame;
        if (!call_frame_has_code(cur_frame)) {
            ctx_reload_post_ret(ctx);
            continue;
        }

        cur_op = &cur_frame->cur_op;
        ctx->increment_pc = UACPI_TRUE;

        st = peek_op(cur_frame);
        if (uacpi_unlikely_error(st))
            goto out;

        trace_op(cur_op);

        switch (cur_op->info.type)
        {
        case UACPI_OPCODE_TYPE_EXEC:
            st = exec_init(ctx);
            goto maybe_dispatch_op;
        case UACPI_OPCODE_TYPE_METHOD_CALL:
            st = method_call_init(ctx);
            goto maybe_dispatch_op;
        case UACPI_OPCODE_TYPE_FLOW:
            st = flow_init(ctx);
            goto next_insn;
        case UACPI_OPCODE_TYPE_ARG:
            cur_frame->code_offset += op_size(cur_op);

            st = get_arg(cur_frame);
            if (uacpi_unlikely_error(st))
                goto out;

            ctx->increment_pc = UACPI_FALSE;
            goto maybe_dispatch_op;
        case UACPI_OPCODE_TYPE_CREATE:
            st = create_dispatch(cur_frame);
            ctx->increment_pc = UACPI_FALSE;
            goto next_insn;
        default:
            uacpi_kernel_log(UACPI_LOG_WARN, "Unimplemented opcode type %u\n",
                             cur_op->info.type);
            st = UACPI_STATUS_UNIMPLEMENTED;
            goto out;
        }
    maybe_dispatch_op:
        if (op_dispatchable(ctx->op_ctx))
            st = dispatch_op(ctx);
    next_insn:
        if (st != UACPI_STATUS_OK)
            goto out;

        if (ctx->increment_pc)
            cur_frame->code_offset += op_size(cur_op);
    }

out:
    execution_context_release(ctx);
    return st;
}
