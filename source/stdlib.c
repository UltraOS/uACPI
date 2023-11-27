#include <uacpi/internal/stdlib.h>

void uacpi_memcpy_zerout(void *dst, const void *src,
                         uacpi_size dst_size, uacpi_size src_size)
{
    if (src_size)
        uacpi_memcpy(dst, src, src_size);

    if (dst_size > src_size)
        uacpi_memzero(dst, dst_size - src_size);
}
