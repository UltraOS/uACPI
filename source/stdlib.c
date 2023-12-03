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
