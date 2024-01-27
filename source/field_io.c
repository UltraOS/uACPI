#include <uacpi/internal/field_io.h>
#include <uacpi/internal/stdlib.h>

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
