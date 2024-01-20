#include <uacpi/types.h>
#include <uacpi/status.h>

#include <uacpi/internal/context.h>
#include <uacpi/internal/utilities.h>
#include <uacpi/internal/log.h>

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
