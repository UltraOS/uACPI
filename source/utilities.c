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
            lvl = UACPI_LOG_ERROR;
        }

        uacpi_log_lvl(
            lvl, "invalid table '%.4s' checksum!\n", (const uacpi_char*)table
        );
    }

    return ret;
}

uacpi_status uacpi_check_tbl_signature_with_warn(
    void *table, const uacpi_char *expect
)
{
    uacpi_status ret = UACPI_STATUS_OK;

    if (uacpi_memcmp(table, expect, 4) != 0) {
        enum uacpi_log_level lvl = UACPI_LOG_WARN;


        if (uacpi_rt_params_check(UACPI_PARAM_BAD_TBL_HDR_FATAL)) {
            ret = UACPI_STATUS_INVALID_SIGNATURE;
            lvl = UACPI_LOG_ERROR;
        }

        uacpi_log_lvl(lvl, "invalid table signature '%.4s' (expected '%.4s')\n",
                      (const uacpi_char*)table, expect);
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

enum char_type {
    CHAR_TYPE_CONTROL = 1 << 0,
    CHAR_TYPE_SPACE = 1 << 1,
    CHAR_TYPE_BLANK = 1 << 2,
    CHAR_TYPE_PUNCTUATION = 1 << 3,
    CHAR_TYPE_LOWER = 1 << 4,
    CHAR_TYPE_UPPER = 1 << 5,
    CHAR_TYPE_DIGIT = 1 << 6,
    CHAR_TYPE_HEX_DIGIT  = 1 << 7,
    CHAR_TYPE_ALPHA = CHAR_TYPE_LOWER | CHAR_TYPE_UPPER,
    CHAR_TYPE_ALHEX = CHAR_TYPE_ALPHA | CHAR_TYPE_HEX_DIGIT,
    CHAR_TYPE_ALNUM = CHAR_TYPE_ALPHA | CHAR_TYPE_DIGIT,
};

static const uacpi_u8 ascii_map[256] = {
    CHAR_TYPE_CONTROL, // 0
    CHAR_TYPE_CONTROL, // 1
    CHAR_TYPE_CONTROL, // 2
    CHAR_TYPE_CONTROL, // 3
    CHAR_TYPE_CONTROL, // 4
    CHAR_TYPE_CONTROL, // 5
    CHAR_TYPE_CONTROL, // 6
    CHAR_TYPE_CONTROL, // 7
    CHAR_TYPE_CONTROL, // -> 8 control codes

    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE | CHAR_TYPE_BLANK, // 9 tab

    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // 10
    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // 11
    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // 12
    CHAR_TYPE_CONTROL | CHAR_TYPE_SPACE, // -> 13 whitespaces

    CHAR_TYPE_CONTROL, // 14
    CHAR_TYPE_CONTROL, // 15
    CHAR_TYPE_CONTROL, // 16
    CHAR_TYPE_CONTROL, // 17
    CHAR_TYPE_CONTROL, // 18
    CHAR_TYPE_CONTROL, // 19
    CHAR_TYPE_CONTROL, // 20
    CHAR_TYPE_CONTROL, // 21
    CHAR_TYPE_CONTROL, // 22
    CHAR_TYPE_CONTROL, // 23
    CHAR_TYPE_CONTROL, // 24
    CHAR_TYPE_CONTROL, // 25
    CHAR_TYPE_CONTROL, // 26
    CHAR_TYPE_CONTROL, // 27
    CHAR_TYPE_CONTROL, // 28
    CHAR_TYPE_CONTROL, // 29
    CHAR_TYPE_CONTROL, // 30
    CHAR_TYPE_CONTROL, // -> 31 control codes

    CHAR_TYPE_SPACE | CHAR_TYPE_BLANK, // 32 space

    CHAR_TYPE_PUNCTUATION, // 33
    CHAR_TYPE_PUNCTUATION, // 34
    CHAR_TYPE_PUNCTUATION, // 35
    CHAR_TYPE_PUNCTUATION, // 36
    CHAR_TYPE_PUNCTUATION, // 37
    CHAR_TYPE_PUNCTUATION, // 38
    CHAR_TYPE_PUNCTUATION, // 39
    CHAR_TYPE_PUNCTUATION, // 40
    CHAR_TYPE_PUNCTUATION, // 41
    CHAR_TYPE_PUNCTUATION, // 42
    CHAR_TYPE_PUNCTUATION, // 43
    CHAR_TYPE_PUNCTUATION, // 44
    CHAR_TYPE_PUNCTUATION, // 45
    CHAR_TYPE_PUNCTUATION, // 46
    CHAR_TYPE_PUNCTUATION, // -> 47 punctuation

    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 48
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 49
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 50
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 51
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 52
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 53
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 54
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 55
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // 56
    CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT, // -> 57 digits

    CHAR_TYPE_PUNCTUATION, // 58
    CHAR_TYPE_PUNCTUATION, // 59
    CHAR_TYPE_PUNCTUATION, // 60
    CHAR_TYPE_PUNCTUATION, // 61
    CHAR_TYPE_PUNCTUATION, // 62
    CHAR_TYPE_PUNCTUATION, // 63
    CHAR_TYPE_PUNCTUATION, // -> 64 punctuation

    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 65
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 66
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 67
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 68
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // 69
    CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT, // -> 70 ABCDEF

    CHAR_TYPE_UPPER, // 71
    CHAR_TYPE_UPPER, // 72
    CHAR_TYPE_UPPER, // 73
    CHAR_TYPE_UPPER, // 74
    CHAR_TYPE_UPPER, // 75
    CHAR_TYPE_UPPER, // 76
    CHAR_TYPE_UPPER, // 77
    CHAR_TYPE_UPPER, // 78
    CHAR_TYPE_UPPER, // 79
    CHAR_TYPE_UPPER, // 80
    CHAR_TYPE_UPPER, // 81
    CHAR_TYPE_UPPER, // 82
    CHAR_TYPE_UPPER, // 83
    CHAR_TYPE_UPPER, // 84
    CHAR_TYPE_UPPER, // 85
    CHAR_TYPE_UPPER, // 86
    CHAR_TYPE_UPPER, // 87
    CHAR_TYPE_UPPER, // 88
    CHAR_TYPE_UPPER, // 89
    CHAR_TYPE_UPPER, // -> 90 the rest of UPPERCASE alphabet

    CHAR_TYPE_PUNCTUATION, // 91
    CHAR_TYPE_PUNCTUATION, // 92
    CHAR_TYPE_PUNCTUATION, // 93
    CHAR_TYPE_PUNCTUATION, // 94
    CHAR_TYPE_PUNCTUATION, // 95
    CHAR_TYPE_PUNCTUATION, // -> 96 punctuation

    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 97
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 98
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 99
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 100
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // 101
    CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT, // -> 102 abcdef

    CHAR_TYPE_LOWER, // 103
    CHAR_TYPE_LOWER, // 104
    CHAR_TYPE_LOWER, // 105
    CHAR_TYPE_LOWER, // 106
    CHAR_TYPE_LOWER, // 107
    CHAR_TYPE_LOWER, // 108
    CHAR_TYPE_LOWER, // 109
    CHAR_TYPE_LOWER, // 110
    CHAR_TYPE_LOWER, // 111
    CHAR_TYPE_LOWER, // 112
    CHAR_TYPE_LOWER, // 113
    CHAR_TYPE_LOWER, // 114
    CHAR_TYPE_LOWER, // 115
    CHAR_TYPE_LOWER, // 116
    CHAR_TYPE_LOWER, // 117
    CHAR_TYPE_LOWER, // 118
    CHAR_TYPE_LOWER, // 119
    CHAR_TYPE_LOWER, // 120
    CHAR_TYPE_LOWER, // 121
    CHAR_TYPE_LOWER, // -> 122 the rest of UPPERCASE alphabet

    CHAR_TYPE_PUNCTUATION, // 123
    CHAR_TYPE_PUNCTUATION, // 124
    CHAR_TYPE_PUNCTUATION, // 125
    CHAR_TYPE_PUNCTUATION, // -> 126 punctuation

    CHAR_TYPE_CONTROL // 127 backspace
};

static inline uacpi_bool is_valid_name_byte(uacpi_u8 c)
{
    // ‘_’ := 0x5F
    if (c == 0x5F)
        return UACPI_TRUE;

    /*
     * LeadNameChar := ‘A’-‘Z’ | ‘_’
     * DigitChar := ‘0’ - ‘9’
     * NameChar := DigitChar | LeadNameChar
     * ‘A’-‘Z’ := 0x41 - 0x5A
     * ‘0’-‘9’ := 0x30 - 0x39
     */
    return (ascii_map[c] & (CHAR_TYPE_DIGIT | CHAR_TYPE_UPPER)) != 0;
}

uacpi_bool uacpi_is_valid_nameseg(uacpi_u8 *nameseg)
{
    return is_valid_name_byte(nameseg[0]) &&
           is_valid_name_byte(nameseg[1]) &&
           is_valid_name_byte(nameseg[2]) &&
           is_valid_name_byte(nameseg[3]);
}

static uacpi_bool is_char(uacpi_char c, enum char_type type)
{
    return (ascii_map[(uacpi_u8)c] & type) == type;
}

static uacpi_char to_lower(uacpi_char c)
{
    if (is_char(c, CHAR_TYPE_UPPER))
        return c + ('a' - 'A');

    return c;
}

static uacpi_bool peek_one(
    const uacpi_char **str, uacpi_size *size, uacpi_char *out_char
)
{
    if (*size == 0)
        return UACPI_FALSE;

    *out_char = **str;
    return UACPI_TRUE;
}

static uacpi_bool consume_one(
    const uacpi_char **str, uacpi_size *size, uacpi_char *out_char
)
{
    if (!peek_one(str, size, out_char))
        return UACPI_FALSE;

    *str += 1;
    *size -= 1;
    return UACPI_TRUE;
}

static uacpi_bool consume_if(
    const uacpi_char **str, uacpi_size *size, enum char_type type
)
{
    uacpi_char c;

    if (!peek_one(str, size, &c) || !is_char(c, type))
        return UACPI_FALSE;

    *str += 1;
    *size -= 1;
    return UACPI_TRUE;
}

static uacpi_bool consume_if_equals(
    const uacpi_char **str, uacpi_size *size, uacpi_char c
)
{
    uacpi_char c1;

    if (!peek_one(str, size, &c1) || to_lower(c1) != c)
        return UACPI_FALSE;

    *str += 1;
    *size -= 1;
    return UACPI_TRUE;
}

uacpi_status uacpi_string_to_integer(
    const uacpi_char *str, uacpi_size max_chars, enum uacpi_base base,
    uacpi_u64 *out_value
)
{
    uacpi_status ret = UACPI_STATUS_INVALID_ARGUMENT;
    uacpi_bool negative = UACPI_FALSE;
    uacpi_u64 next, value = 0;
    uacpi_char c = '\0';

    while (consume_if(&str, &max_chars, CHAR_TYPE_SPACE));

    if (consume_if_equals(&str, &max_chars, '-'))
        negative = UACPI_TRUE;
    else
        consume_if_equals(&str, &max_chars, '+');

    if (base == UACPI_BASE_AUTO) {
        base = UACPI_BASE_DEC;

        if (consume_if_equals(&str, &max_chars, '0')) {
            base = UACPI_BASE_OCT;
            if (consume_if_equals(&str, &max_chars, 'x'))
                base = UACPI_BASE_HEX;
        }
    }

    while (consume_one(&str, &max_chars, &c)) {
        switch (ascii_map[(uacpi_u8)c] & (CHAR_TYPE_DIGIT | CHAR_TYPE_ALHEX)) {
        case CHAR_TYPE_DIGIT | CHAR_TYPE_HEX_DIGIT:
            next = c - '0';
            if (base == UACPI_BASE_OCT && next > 7)
                goto out;
            break;
        case CHAR_TYPE_LOWER | CHAR_TYPE_HEX_DIGIT:
        case CHAR_TYPE_UPPER | CHAR_TYPE_HEX_DIGIT:
            if (base != UACPI_BASE_HEX)
                goto out;
            next = 10 + (to_lower(c) - 'a');
            break;
        default:
            goto out;
        }

        next = (value * base) + next;
        if ((next / base) != value) {
            value = 0xFFFFFFFFFFFFFFFF;
            goto out;
        }

        value = next;
    }

out:
    if (negative)
        value = -((uacpi_i64)value);

    *out_value = value;
    if (max_chars == 0 || c == '\0')
        ret = UACPI_STATUS_OK;

    return ret;
}

#define PNP_ID_LENGTH 8

uacpi_status uacpi_eval_hid(uacpi_namespace_node *node, uacpi_id_string **out_id)
{
    uacpi_status ret;
    uacpi_object *hid_ret;
    uacpi_id_string *id;
    uacpi_u32 size;

    ret = uacpi_eval_typed(
        node, "_HID", UACPI_NULL,
        UACPI_OBJECT_INTEGER_BIT | UACPI_OBJECT_STRING_BIT,
        &hid_ret
    );
    if (ret != UACPI_STATUS_OK)
        return ret;

    size = sizeof(uacpi_id_string);

    switch (hid_ret->type) {
    case UACPI_OBJECT_STRING: {
        uacpi_buffer *buf = hid_ret->buffer;

        size += buf->size;
        if (uacpi_unlikely(buf->size == 0 || size < buf->size)) {
            uacpi_error(
                "%.4s._HID: empty/invalid EISA ID string (%zu bytes)\n",
                uacpi_namespace_node_name(node).text, buf->size
            );
            ret = UACPI_STATUS_AML_BAD_ENCODING;
            break;
        }

        id = uacpi_kernel_alloc(size);
        if (uacpi_unlikely(id == UACPI_NULL)) {
            ret = UACPI_STATUS_OUT_OF_MEMORY;
            break;
        }
        id->size = buf->size;
        id->value = UACPI_PTR_ADD(id, sizeof(uacpi_id_string));

        uacpi_memcpy(id->value, buf->text, buf->size);
        id->value[buf->size - 1] = '\0';
        break;
    }

    case UACPI_OBJECT_INTEGER:
        size += PNP_ID_LENGTH;

        id = uacpi_kernel_alloc(size);
        if (uacpi_unlikely(id == UACPI_NULL)) {
            ret = UACPI_STATUS_OUT_OF_MEMORY;
            break;
        }
        id->size = PNP_ID_LENGTH;
        id->value = UACPI_PTR_ADD(id, sizeof(uacpi_id_string));

        uacpi_eisa_id_to_string(hid_ret->integer, id->value);
        break;
    }

    uacpi_object_unref(hid_ret);
    if (uacpi_likely_success(ret))
        *out_id = id;
    return ret;
}

void uacpi_free_id_string(uacpi_id_string *id)
{
    if (id == UACPI_NULL)
        return;

    uacpi_free(id, sizeof(uacpi_id_string) + id->size);
}

uacpi_status uacpi_eval_cid(
    uacpi_namespace_node *node, uacpi_pnp_id_list **out_list
)
{
    uacpi_status ret;
    uacpi_object *object, *cid_ret;
    uacpi_object **objects;
    uacpi_size num_ids, i;
    uacpi_u32 size;
    uacpi_id_string *id;
    uacpi_char *id_buffer;
    uacpi_pnp_id_list *list;

    ret = uacpi_eval_typed(
        node, "_CID", UACPI_NULL,
        UACPI_OBJECT_INTEGER_BIT | UACPI_OBJECT_STRING_BIT |
        UACPI_OBJECT_PACKAGE_BIT,
        &cid_ret
    );
    if (ret != UACPI_STATUS_OK)
        return ret;

    switch (cid_ret->type) {
    case UACPI_OBJECT_PACKAGE:
        objects = cid_ret->package->objects;
        num_ids = cid_ret->package->count;
        break;
    default:
        objects = &cid_ret;
        num_ids = 1;
        break;
    }

    size = sizeof(uacpi_pnp_id_list);
    size += num_ids * sizeof(uacpi_id_string);

    for (i = 0; i < num_ids; ++i) {
        object = objects[i];

        switch (object->type) {
        case UACPI_OBJECT_STRING: {
            uacpi_size buf_size = object->buffer->size;

            if (uacpi_unlikely(buf_size == 0)) {
                uacpi_error(
                    "%.4s._CID: empty EISA ID string (sub-object %zu)\n",
                    uacpi_namespace_node_name(node).text, i
                );
                return UACPI_STATUS_AML_INCOMPATIBLE_OBJECT_TYPE;
            }

            size += buf_size;
            if (uacpi_unlikely(size < buf_size)) {
                uacpi_error(
                    "%.4s._CID: buffer size overflow (+ %zu)\n",
                    uacpi_namespace_node_name(node).text, buf_size
                );
                return UACPI_STATUS_AML_BAD_ENCODING;
            }

            break;
        }

        case UACPI_OBJECT_INTEGER:
            size += PNP_ID_LENGTH;
            break;
        default:
            uacpi_error(
                "%.4s._CID: invalid package sub-object %zu type: %s\n",
                uacpi_namespace_node_name(node).text, i,
                uacpi_object_type_to_string(object->type)
            );
            return UACPI_STATUS_AML_INCOMPATIBLE_OBJECT_TYPE;
        }
    }

    list = uacpi_kernel_alloc(size);
    if (uacpi_unlikely(list == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;
    list->num_ids = num_ids;
    list->size = size - sizeof(uacpi_pnp_id_list);

    id_buffer = UACPI_PTR_ADD(list, sizeof(uacpi_pnp_id_list));
    id_buffer += num_ids * sizeof(uacpi_id_string);

    for (i = 0; i < num_ids; ++i) {
        object = objects[i];
        id = &list->ids[i];

        switch (object->type) {
        case UACPI_OBJECT_STRING: {
            uacpi_buffer *buf = object->buffer;

            id->size = buf->size;
            id->value = id_buffer;

            uacpi_memcpy(id->value, buf->text, id->size);
            id->value[id->size - 1] = '\0';
            break;
        }

        case UACPI_OBJECT_INTEGER:
            id->size = PNP_ID_LENGTH;
            id->value = id_buffer;
            uacpi_eisa_id_to_string(object->integer, id_buffer);
            break;
        }

        id_buffer += id->size;
    }

    uacpi_object_unref(cid_ret);
    *out_list = list;
    return ret;
}

void uacpi_free_pnp_id_list(uacpi_pnp_id_list *list)
{
    if (list == UACPI_NULL)
        return;

    uacpi_free(list, sizeof(uacpi_pnp_id_list) + list->size);
}

uacpi_status uacpi_eval_sta(uacpi_namespace_node *node, uacpi_u32 *flags)
{
    uacpi_status ret;
    uacpi_object *obj;

    ret = uacpi_eval_typed(node, "_STA", NULL, UACPI_OBJECT_INTEGER_BIT, &obj);

    /*
     * ACPI 6.5 specification:
     * If a device object (including the processor object) does not have
     * an _STA object, then OSPM assumes that all of the above bits are
     * set (i.e., the device is present, enabled, shown in the UI,
     * and functioning).
     */
    if (ret == UACPI_STATUS_NOT_FOUND) {
        *flags = 0xFFFFFFFF;
        return UACPI_STATUS_OK;
    }

    if (ret == UACPI_STATUS_OK) {
        *flags = obj->integer;
        uacpi_object_unref(obj);
        return ret;
    }

    *flags = 0;
    return ret;
}

static uacpi_bool matches_any(
    uacpi_id_string *id, const uacpi_char **ids
)
{
    uacpi_size i;

    for (i = 0; ids[i]; ++i) {
        if (uacpi_strcmp(id->value, ids[i]) == 0)
            return UACPI_TRUE;
    }

    return UACPI_FALSE;
}

uacpi_bool uacpi_device_matches_pnp_id(
    uacpi_namespace_node *node, const uacpi_char **ids
)
{
    uacpi_status st;
    uacpi_bool ret = UACPI_FALSE;
    uacpi_id_string *id = UACPI_NULL;
    uacpi_pnp_id_list *id_list = UACPI_NULL;

    st = uacpi_eval_hid(node, &id);
    if (st == UACPI_STATUS_OK && matches_any(id, ids)) {
        ret = UACPI_TRUE;
        goto out;
    }

    st = uacpi_eval_cid(node, &id_list);
    if (st == UACPI_STATUS_OK) {
        uacpi_size i;

        for (i = 0; i < id_list->num_ids; ++i) {
            if (matches_any(&id_list->ids[i], ids)) {
                ret = UACPI_TRUE;
                goto out;
            }
        }
    }

out:
    uacpi_free_id_string(id);
    uacpi_free_pnp_id_list(id_list);
    return ret;
}

struct device_find_ctx {
    const uacpi_char **target_hids;
    void *user;
    uacpi_iteration_callback cb;
};

enum uacpi_ns_iteration_decision find_one_device(
    void *opaque, uacpi_namespace_node *node
)
{
    struct device_find_ctx *ctx = opaque;
    uacpi_status ret;
    uacpi_u32 flags;
    uacpi_object *obj;

    obj = uacpi_namespace_node_get_object(node);
    if (uacpi_unlikely(obj == UACPI_NULL))
        return UACPI_NS_ITERATION_DECISION_CONTINUE;
    if (obj->type != UACPI_OBJECT_DEVICE)
        return UACPI_NS_ITERATION_DECISION_CONTINUE;

    if (!uacpi_device_matches_pnp_id(node, ctx->target_hids))
        return UACPI_NS_ITERATION_DECISION_CONTINUE;

    ret = uacpi_eval_sta(node, &flags);
    if (uacpi_unlikely_error(ret))
        return UACPI_NS_ITERATION_DECISION_NEXT_PEER;

    if (!(flags & ACPI_STA_RESULT_DEVICE_PRESENT) &&
        !(flags & ACPI_STA_RESULT_DEVICE_FUNCTIONING))
        return UACPI_NS_ITERATION_DECISION_NEXT_PEER;

    return ctx->cb(ctx->user, node);
}


uacpi_status uacpi_find_devices_at(
    uacpi_namespace_node *parent, const uacpi_char **hids,
    uacpi_iteration_callback cb, void *user
)
{
    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level <
                       UACPI_INIT_LEVEL_NAMESPACE_LOADED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    struct device_find_ctx ctx = {
        .target_hids = hids,
        .user = user,
        .cb = cb,
    };

    uacpi_namespace_for_each_node_depth_first(parent, find_one_device, &ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_find_devices(
    const uacpi_char *hid, uacpi_iteration_callback cb, void *user
)
{
    const uacpi_char *hids[] = {
        hid, UACPI_NULL
    };

    return uacpi_find_devices_at(uacpi_namespace_root(), hids, cb, user);
}

uacpi_status uacpi_set_interrupt_model(uacpi_interrupt_model model)
{
    uacpi_status ret;
    uacpi_object *arg;
    uacpi_args args;

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level <
                       UACPI_INIT_LEVEL_NAMESPACE_LOADED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    arg = uacpi_create_object(UACPI_OBJECT_INTEGER);
    if (uacpi_unlikely(arg == UACPI_NULL))
        return UACPI_STATUS_OUT_OF_MEMORY;

    arg->integer = model;
    args.objects = &arg;
    args.count = 1;

    ret = uacpi_eval(uacpi_namespace_root(), "_PIC", &args, UACPI_NULL);
    uacpi_object_unref(arg);

    if (ret == UACPI_STATUS_NOT_FOUND)
        ret = UACPI_STATUS_OK;

    return ret;
}

uacpi_status uacpi_get_pci_routing_table(
    uacpi_namespace_node *parent, uacpi_pci_routing_table **out_table
)
{
    uacpi_status ret;
    uacpi_object *obj, *entry_obj, *elem_obj;
    uacpi_package *table_pkg, *entry_pkg;
    uacpi_pci_routing_table_entry *entry;
    uacpi_pci_routing_table *table;
    uacpi_size size, i;

    if (uacpi_unlikely(g_uacpi_rt_ctx.init_level <
                       UACPI_INIT_LEVEL_NAMESPACE_LOADED))
        return UACPI_STATUS_INIT_LEVEL_MISMATCH;

    obj = uacpi_namespace_node_get_object(parent);
    if (uacpi_unlikely(obj == UACPI_NULL || obj->type != UACPI_OBJECT_DEVICE))
        return UACPI_STATUS_INVALID_ARGUMENT;

    ret = uacpi_eval_typed(
        parent, "_PRT", UACPI_NULL, UACPI_OBJECT_PACKAGE_BIT, &obj
    );
    if (uacpi_unlikely_error(ret))
        return ret;

    table_pkg = obj->package;
    if (uacpi_unlikely(table_pkg->count == 0 || table_pkg->count > 1024)) {
        uacpi_warn("invalid number of _PRT entries: %zu\n", table_pkg->count);
        uacpi_object_unref(obj);
        return UACPI_STATUS_AML_BAD_ENCODING;
    }

    size = table_pkg->count * sizeof(uacpi_pci_routing_table_entry);
    table = uacpi_kernel_alloc(sizeof(uacpi_pci_routing_table) + size);
    if (uacpi_unlikely(table == UACPI_NULL)) {
        uacpi_object_unref(obj);
        return UACPI_STATUS_OUT_OF_MEMORY;
    }
    table->num_entries = table_pkg->count;

    for (i = 0; i < table_pkg->count; ++i) {
        entry_obj = table_pkg->objects[i];

        if (uacpi_unlikely(entry_obj->type != UACPI_OBJECT_PACKAGE)) {
            uacpi_error("_PRT sub-object %zu is not a package: %s\n",
                        i, uacpi_object_type_to_string(entry_obj->type));
            goto out_bad_encoding;
        }

        entry_pkg = entry_obj->package;
        if (uacpi_unlikely(entry_pkg->count != 4)) {
            uacpi_warn("invalid _PRT sub-package entry count %zu\n",
                       entry_pkg->count);
            goto out_bad_encoding;
        }

        entry = &table->entries[i];

        elem_obj = entry_pkg->objects[0];
        if (uacpi_unlikely(elem_obj->type != UACPI_OBJECT_INTEGER)) {
            uacpi_error("invalid _PRT sub-package %zu address type: %s\n",
                        i, uacpi_object_type_to_string(elem_obj->type));
            goto out_bad_encoding;
        }
        entry->address = elem_obj->integer;

        elem_obj = entry_pkg->objects[1];
        if (uacpi_unlikely(elem_obj->type != UACPI_OBJECT_INTEGER)) {
            uacpi_error("invalid _PRT sub-package %zu pin type: %s\n",
                        i, uacpi_object_type_to_string(elem_obj->type));
            goto out_bad_encoding;
        }
        entry->pin = elem_obj->integer;

        elem_obj = entry_pkg->objects[2];
        switch (elem_obj->type) {
        case UACPI_OBJECT_STRING:
            entry->source = uacpi_namespace_node_resolve_from_aml_namepath(
                parent, elem_obj->buffer->text
            );
            if (uacpi_unlikely(entry->source == UACPI_NULL)) {
                uacpi_error("unable to lookup _PRT source: %s\n",
                            elem_obj->buffer->text);
                goto out_bad_encoding;
            }
            break;
        case UACPI_OBJECT_INTEGER:
            entry->source = UACPI_NULL;
            break;
        default:
            uacpi_error("invalid _PRT sub-package %zu source type: %s\n",
                        i, uacpi_object_type_to_string(elem_obj->type));
            goto out_bad_encoding;
        }

        elem_obj = entry_pkg->objects[3];
        if (uacpi_unlikely(elem_obj->type != UACPI_OBJECT_INTEGER)) {
            uacpi_error("invalid _PRT sub-package %zu source index type: %s\n",
                        i, uacpi_object_type_to_string(elem_obj->type));
            goto out_bad_encoding;
        }
        entry->index = elem_obj->integer;
    }

    uacpi_object_unref(obj);
    *out_table = table;
    return UACPI_STATUS_OK;

out_bad_encoding:
    uacpi_object_unref(obj);
    uacpi_free_pci_routing_table(table);
    return UACPI_STATUS_AML_BAD_ENCODING;
}

void uacpi_free_pci_routing_table(uacpi_pci_routing_table *table)
{
    if (table == UACPI_NULL)
        return;

    uacpi_free(
        table,
        sizeof(uacpi_pci_routing_table) +
        table->num_entries * sizeof(uacpi_pci_routing_table_entry)
    );
}

void uacpi_free_dynamic_string(const uacpi_char *str)
{
    if (str == UACPI_NULL)
        return;

    uacpi_free((void*)str, uacpi_strlen(str) + 1);
}
