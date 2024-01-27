#include <uacpi/internal/field_io.h>
#include <uacpi/internal/stdlib.h>
#include <uacpi/internal/log.h>
#include <uacpi/internal/opregion.h>

uacpi_size uacpi_round_up_bits_to_bytes(uacpi_size bit_length)
{
    return UACPI_ALIGN_UP(bit_length, 8, uacpi_size) / 8;
}

static void cut_misaligned_tail(
    uacpi_u8 *data, uacpi_size offset, uacpi_u32 bit_length
)
{
    uacpi_u8 remainder = bit_length & 7;

    if (remainder == 0)
        return;

    data[offset] &= ((1ull << remainder) - 1);
}

struct bit_span
{
    union {
        uacpi_u8 *data;
        const uacpi_u8 *const_data;
    };
    uacpi_u64 index;
    uacpi_u64 length;
};

static uacpi_size bit_span_offset(struct bit_span *span, uacpi_size bits)
{
    uacpi_size delta = UACPI_MIN(span->length, bits);

    span->index += delta;
    span->length -= delta;

    return delta;
}

static void bit_copy(struct bit_span *dst, struct bit_span *src)
{
    uacpi_u8 src_shift, dst_shift, bits = 0;
    uacpi_u16 dst_mask;
    uacpi_u8 *dst_ptr, *src_ptr;
    uacpi_u64 dst_count, src_count;

    dst_ptr = dst->data + (dst->index / 8);
    src_ptr = src->data + (src->index / 8);

    dst_count = dst->length;
    dst_shift = dst->index & 7;

    src_count = src->length;
    src_shift = src->index & 7;

    while (dst_count)
    {
        bits = 0;

        if (src_count) {
            bits = *src_ptr >> src_shift;

            if (src_shift && src_count > 8 - src_shift)
                bits |= *(src_ptr + 1) << (8 - src_shift);

            if (src_count < 8) {
                bits &= (1 << src_count) - 1;
                src_count = 0;
            } else {
                src_count -= 8;
                src_ptr++;
            }
        }

        dst_mask = (dst_count < 8 ? (1 << dst_count) - 1 : 0xFF) << dst_shift;
        *dst_ptr = (*dst_ptr & ~dst_mask) | ((bits << dst_shift) & dst_mask);

        if (dst_shift && dst_count > (8 - dst_shift)) {
            dst_mask >>= 8;
            *(dst_ptr + 1) &= ~dst_mask;
            *(dst_ptr + 1) |= (bits >> (8 - dst_shift)) & dst_mask;
        }

        dst_count = dst_count > 8 ? dst_count - 8 : 0;
        ++dst_ptr;
    }
}

static void do_misaligned_buffer_read(
    const uacpi_buffer_field *field, uacpi_u8 *dst
)
{
    struct bit_span src_span = {
        .index = field->bit_index,
        .length = field->bit_length,
        .const_data = field->backing->data,
    };
    struct bit_span dst_span = {
        .data = dst,
    };

    dst_span.length = uacpi_round_up_bits_to_bytes(field->bit_length) * 8;
    bit_copy(&dst_span, &src_span);
}

void uacpi_read_buffer_field(
    const uacpi_buffer_field *field, void *dst
)
{
    if (!(field->bit_index & 7)) {
        uacpi_u8 *src = field->backing->data;
        uacpi_size count;

        count = uacpi_round_up_bits_to_bytes(field->bit_length);
        uacpi_memcpy(dst, src + (field->bit_index / 8), count);
        cut_misaligned_tail(dst, count - 1, field->bit_length);
        return;
    }

    do_misaligned_buffer_read(field, dst);
}

static void do_write_misaligned_buffer_field(
    uacpi_buffer_field *field,
    const void *src, uacpi_size size
)
{
    struct bit_span src_span = {
        .length = size * 8,
        .const_data = src,
    };
    struct bit_span dst_span = {
        .index = field->bit_index,
        .length = field->bit_length,
        .data = field->backing->data,
    };

    bit_copy(&dst_span, &src_span);
}

void uacpi_write_buffer_field(
    uacpi_buffer_field *field,
    const void *src, uacpi_size size
)
{
    if (!(field->bit_index & 7)) {
        uacpi_u8 *dst, last_byte, tail_shift;
        uacpi_size count;

        dst = field->backing->data;
        dst += field->bit_index / 8;
        count = uacpi_round_up_bits_to_bytes(field->bit_length);

        last_byte = dst[count - 1];
        tail_shift = field->bit_length & 7;

        uacpi_memcpy_zerout(dst, src, count, size);
        if (tail_shift) {
            uacpi_u8 last_shift = 8 - tail_shift;
            dst[count - 1] = dst[count - 1] << last_shift;
            dst[count - 1] >>= last_shift;
            dst[count - 1] |= (last_byte >> tail_shift) << tail_shift;
        }

        return;
    }

    do_write_misaligned_buffer_field(field, src, size);
}

static uacpi_status dispatch_field_io(
    uacpi_namespace_node *region_node, uacpi_u32 offset, uacpi_u8 byte_width,
    uacpi_region_op op, uacpi_u64 *in_out
)
{
    uacpi_status ret;
    uacpi_operation_region *region;
    uacpi_address_space_handler *handler;

    uacpi_region_rw_data data = {
        .byte_width = byte_width,
        .offset = offset,
    };

    ret = uacpi_opregion_attach(region_node);
    if (uacpi_unlikely_error(ret))
        return ret;

    region = uacpi_namespace_node_get_object(region_node)->op_region;

    handler = region->handler;

    data.offset += region->offset;
    data.handler_context = handler->user_context;
    data.region_context = region->user_context;

    if (op == UACPI_REGION_OP_WRITE) {
        data.value = *in_out;
        uacpi_trace_region_io(region_node, op, data.offset,
                              byte_width, data.value);
    }

    ret = handler->callback(op, &data);
    if (uacpi_unlikely_error(ret))
        return ret;

    if (op == UACPI_REGION_OP_READ) {
        *in_out = data.value;
        uacpi_trace_region_io(region_node, op, data.offset,
                              byte_width, data.value);
    }

    return UACPI_STATUS_OK;
}

static uacpi_status access_field_unit(
    uacpi_field_unit *field, uacpi_u32 offset, uacpi_region_op op,
    uacpi_u64 *in_out
)
{
    uacpi_status ret = UACPI_STATUS_OK;
    uacpi_namespace_node *region_node;

    switch (field->kind) {
    case UACPI_FIELD_UNIT_KIND_BANK:
        ret = uacpi_write_field_unit(
            field->bank_selection, &field->bank_value, sizeof(field->bank_value)
        );
        region_node = field->bank_region;
        break;
    case UACPI_FIELD_UNIT_KIND_NORMAL:
        region_node = field->region;
        break;
    case UACPI_FIELD_UNIT_KIND_INDEX:
        ret = uacpi_write_field_unit(
            field->index, &offset, sizeof(offset)
        );
        if (uacpi_unlikely_error(ret))
            return ret;

        switch (op) {
        case UACPI_REGION_OP_READ:
            return uacpi_read_field_unit(
                field->data, in_out, field->access_width_bytes
            );
        case UACPI_REGION_OP_WRITE:
            return uacpi_write_field_unit(
                field->data, in_out, field->access_width_bytes
            );
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
        }

    default:
        uacpi_error("invalid field unit kind %d\n", field->kind);
        ret = UACPI_STATUS_INVALID_ARGUMENT;
    }

    if (uacpi_unlikely_error(ret))
        return ret;

    return dispatch_field_io(
        region_node, offset, field->access_width_bytes, op, in_out
    );
}

static uacpi_status do_read_misaligned_field_unit(
    uacpi_field_unit *field, uacpi_u8 *dst, uacpi_size size
)
{
    uacpi_status ret;
    uacpi_size reads_to_do;
    uacpi_u64 out;
    uacpi_u32 byte_offset = field->byte_offset;
    uacpi_u32 bits_left = field->bit_length;
    uacpi_u8 width_access_bits = field->access_width_bytes * 8;

    struct bit_span src_span = {
        .data = (uacpi_u8*)&out,
        .index = field->bit_offset_within_first_byte,
    };
    struct bit_span dst_span = {
        .data = dst,
        .index = 0,
        .length = size * 8
    };

    reads_to_do = UACPI_ALIGN_UP(
        field->bit_offset_within_first_byte + field->bit_length,
        width_access_bits,
        uacpi_u32
    );
    reads_to_do /= width_access_bits;

    while (reads_to_do-- > 0) {
        src_span.length = UACPI_MIN(
            bits_left, width_access_bits - src_span.index
        );

        ret = access_field_unit(
            field, byte_offset, UACPI_REGION_OP_READ,
            &out
        );
        if (uacpi_unlikely_error(ret))
            return ret;

        bit_copy(&dst_span, &src_span);
        bits_left -= src_span.length;
        src_span.index = 0;

        bit_span_offset(&dst_span, src_span.length);
        byte_offset += field->access_width_bytes;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_read_field_unit(
    uacpi_field_unit *field, void *dst, uacpi_size size
)
{
    uacpi_status ret;
    uacpi_u32 field_byte_length;

    field_byte_length = uacpi_round_up_bits_to_bytes(field->bit_length);

    /*
     * Very simple fast case:
     * - Bit offset within first byte is 0
     * AND
     * - Field size is <= access width
     */
    if (field->bit_offset_within_first_byte == 0 &&
        field_byte_length <= field->access_width_bytes)
    {
        uacpi_u64 out;

        ret = access_field_unit(
            field, field->byte_offset, UACPI_REGION_OP_READ, &out
        );
        if (uacpi_unlikely_error(ret))
            return ret;

        uacpi_memcpy_zerout(dst, &out, size, field_byte_length);
        if (size >= field_byte_length)
            cut_misaligned_tail(dst, field_byte_length - 1, field->bit_length);

        return UACPI_STATUS_OK;
    }

    // Slow case
    return do_read_misaligned_field_unit(field, dst, size);
}

uacpi_status uacpi_write_field_unit(
    uacpi_field_unit *field, const void *src, uacpi_size size
)
{
    uacpi_status ret;
    uacpi_u32 bits_left, byte_offset = field->byte_offset;
    uacpi_u8 width_access_bits = field->access_width_bytes * 8;
    uacpi_u64 in;

    struct bit_span src_span = {
        .const_data = src,
        .index = 0,
        .length = size * 8
    };
    struct bit_span dst_span = {
        .data = (uacpi_u8*)&in,
        .index = field->bit_offset_within_first_byte,
    };

    bits_left = field->bit_length;

    while (bits_left) {
        in = 0;
        dst_span.length = UACPI_MIN(
            width_access_bits - dst_span.index, bits_left
        );

        if (dst_span.index != 0 || dst_span.length < width_access_bits) {
            switch (field->update_rule) {
            case UACPI_UPDATE_RULE_PRESERVE:
                ret = access_field_unit(
                    field, byte_offset, UACPI_REGION_OP_READ, &in
                );
                if (uacpi_unlikely_error(ret))
                    return ret;
                break;
            case UACPI_UPDATE_RULE_WRITE_AS_ONES:
                in = ~in;
                break;
            case UACPI_UPDATE_RULE_WRITE_AS_ZEROES:
                break;
            default:
                uacpi_error("invalid field@%p update rule %d\n",
                            field, field->update_rule);
                return UACPI_STATUS_INVALID_ARGUMENT;
            }
        }

        bit_copy(&dst_span, &src_span);
        bit_span_offset(&src_span, dst_span.length);

        ret = access_field_unit(
            field, byte_offset, UACPI_REGION_OP_WRITE, &in
        );
        if (uacpi_unlikely_error(ret))
            return ret;

        bits_left -= dst_span.length;
        dst_span.index = 0;
        byte_offset += field->access_width_bytes;
    }

    return UACPI_STATUS_OK;
}
