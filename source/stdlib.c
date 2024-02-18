#include <uacpi/internal/stdlib.h>

void uacpi_memcpy_zerout(void *dst, const void *src,
                         uacpi_size dst_size, uacpi_size src_size)
{
    uacpi_size bytes_to_copy = UACPI_MIN(src_size, dst_size);

    if (bytes_to_copy)
        uacpi_memcpy(dst, src, bytes_to_copy);

    if (dst_size > bytes_to_copy)
        uacpi_memzero((uacpi_u8*)dst + bytes_to_copy, dst_size - bytes_to_copy);
}

uacpi_u8 uacpi_bit_scan_forward(uacpi_u64 value)
{
#ifdef _MSC_VER
    unsigned char ret;
    unsigned long index;

    ret = _BitScanForward64(&index, value);
    if (ret == 0)
        return 0;

    return (uacpi_u8)index + 1;
#else
    return __builtin_ffsll(value);
#endif
}

uacpi_u8 uacpi_bit_scan_backward(uacpi_u64 value)
{
#ifdef _MSC_VER
    unsigned char ret;
    unsigned long index;

    ret = _BitScanReverse64(&index, value);
    if (ret == 0)
        return 0;

    return (uacpi_u8)index + 1;
#else
    if (value == 0)
        return 0;

    return 64 - __builtin_clzll(value);
#endif
}

uacpi_u8 uacpi_popcount(uacpi_u64 value)
{
#ifdef _MSC_VER
    return __popcnt64(value);
#else
    return __builtin_popcountll(value);
#endif
}
