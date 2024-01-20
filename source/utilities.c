#include <uacpi/types.h>
#include <uacpi/status.h>

#include <uacpi/internal/context.h>
#include <uacpi/internal/utilities.h>
#include <uacpi/internal/log.h>
#include <uacpi/uacpi.h>

static uacpi_u8 uacpi_table_checksum(void *table, uacpi_size size)
{
    uacpi_u8 *bytes = table;
    uacpi_u8 csum = 0;
    uacpi_size i;

    for (i = 0; i < size; ++i)
        csum += bytes[i];

    return csum;
}

uacpi_status uacpi_verify_table_checksum_with_warn(void *table, uacpi_size size)
{
    uacpi_status ret = UACPI_STATUS_OK;

    if (uacpi_table_checksum(table, size) != 0) {
        enum uacpi_log_level lvl = UACPI_LOG_WARN;

        if (uacpi_rt_params_check(UACPI_PARAM_BAD_CSUM_FATAL)) {
            ret = UACPI_STATUS_BAD_CHECKSUM;
            lvl = UACPI_LOG_FATAL;
        }

        uacpi_log_lvl(lvl, "invalid table '%.4s' checksum!\n", (const char*)table);
    }

    return ret;
}

uacpi_status uacpi_check_tbl_signature_with_warn(void *table, const char *expect)
{
    uacpi_status ret = UACPI_STATUS_OK;

    if (uacpi_memcmp(table, expect, 4) != 0) {
        enum uacpi_log_level lvl = UACPI_LOG_WARN;


        if (uacpi_rt_params_check(UACPI_PARAM_BAD_TBL_HDR_FATAL)) {
            ret = UACPI_STATUS_INVALID_SIGNATURE;
            lvl = UACPI_LOG_FATAL;
        }

        uacpi_log_lvl(lvl, "invalid table signature '%.4s' (expected '%.4s')\n",
                      (const char*)table, expect);
    }

    return ret;
}

void uacpi_eisa_id_to_string(uacpi_u32 id, uacpi_char *out_string)
{
    static uacpi_char hex_to_ascii[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F'
    };

    /*
     * For whatever reason bits are encoded upper to lower here, swap
     * them around so that we don't have to do ridiculous bit shifts
     * everywhere.
     */
    union {
        uacpi_u8 bytes[4];
        uacpi_u32 dword;
    } orig, swapped;

    orig.dword = id;
    swapped.bytes[0] = orig.bytes[3];
    swapped.bytes[1] = orig.bytes[2];
    swapped.bytes[2] = orig.bytes[1];
    swapped.bytes[3] = orig.bytes[0];

    /*
     * Bit 16 - 20: 3rd character (- 0x40) of mfg code
     * Bit 21 - 25: 2nd character (- 0x40) of mfg code
     * Bit 26 - 30: 1st character (- 0x40) of mfg code
     */
    out_string[0] = (uacpi_char)(0x40 + ((swapped.dword >> 26) & 0x1F));
    out_string[1] = (uacpi_char)(0x40 + ((swapped.dword >> 21) & 0x1F));
    out_string[2] = (uacpi_char)(0x40 + ((swapped.dword >> 16) & 0x1F));

    /*
     * Bit 0  - 3 : 4th hex digit of product number
     * Bit 4  - 7 : 3rd hex digit of product number
     * Bit 8  - 11: 2nd hex digit of product number
     * Bit 12 - 15: 1st hex digit of product number
     */
    out_string[3] = hex_to_ascii[(swapped.dword >> 12) & 0x0F];
    out_string[4] = hex_to_ascii[(swapped.dword >> 8 ) & 0x0F];
    out_string[5] = hex_to_ascii[(swapped.dword >> 4 ) & 0x0F];
    out_string[6] = hex_to_ascii[(swapped.dword >> 0 ) & 0x0F];

    out_string[7] = '\0';
}

static uacpi_char *steal_or_copy_string(uacpi_object *obj)
{
    uacpi_char *ret;

    if (uacpi_shareable_refcount(obj) == 1 &&
        uacpi_shareable_refcount(obj->buffer) == 1) {
        // No need to allocate anything, we can just steal the returned buffer
        ret = obj->buffer->text;
        obj->buffer->text = UACPI_NULL;
        obj->buffer->size = 0;
    } else {
        ret = uacpi_kernel_alloc(obj->buffer->size);
        if (uacpi_unlikely(ret == UACPI_NULL))
            return UACPI_NULL;

        uacpi_memcpy(ret, obj->buffer->text, obj->buffer->size);
    }

    return ret;
}

uacpi_status uacpi_eval_hid(uacpi_namespace_node *node, uacpi_char **out_hid)
{
    uacpi_status ret;
    uacpi_object *hid_ret;
    uacpi_char *out_id = UACPI_NULL;

    ret = uacpi_eval_typed(
        node, "_HID", UACPI_NULL,
        UACPI_OBJECT_INTEGER_BIT | UACPI_OBJECT_STRING_BIT,
        &hid_ret
    );
    if (ret != UACPI_STATUS_OK)
        return ret;

    switch (hid_ret->type) {
    case UACPI_OBJECT_STRING:
        out_id = steal_or_copy_string(hid_ret);
        if (uacpi_unlikely(out_id == UACPI_NULL))
            ret = UACPI_STATUS_OUT_OF_MEMORY;
        break;
    case UACPI_OBJECT_INTEGER:
        out_id = uacpi_kernel_alloc(8);
        if (uacpi_unlikely(out_id == UACPI_NULL)) {
            ret = UACPI_STATUS_OUT_OF_MEMORY;
            break;
        }

        uacpi_eisa_id_to_string(hid_ret->integer, out_id);
        break;
    }

    *out_hid = out_id;
    uacpi_object_unref(hid_ret);
    return ret;
}