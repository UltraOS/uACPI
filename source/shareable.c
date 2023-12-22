#include <uacpi/internal/shareable.h>

#define BUGGED_REFCOUNT 0xFFFFFFFF

void uacpi_shareable_init(uacpi_handle handle)
{
    struct uacpi_shareable *shareable = handle;
    shareable->reference_count = 1;
}

uacpi_bool uacpi_bugged_shareable(uacpi_handle handle)
{
    struct uacpi_shareable *shareable = handle;

    if (uacpi_unlikely(shareable->reference_count == 0))
        shareable->reference_count = BUGGED_REFCOUNT;

    return shareable->reference_count == BUGGED_REFCOUNT;
}

void uacpi_make_shareable_bugged(uacpi_handle handle)
{
    struct uacpi_shareable *shareable = handle;
    shareable->reference_count = BUGGED_REFCOUNT;
}

uacpi_u32 uacpi_shareable_ref(uacpi_handle handle)
{
    struct uacpi_shareable *shareable = handle;

    if (uacpi_unlikely(uacpi_bugged_shareable(shareable)))
        return BUGGED_REFCOUNT;

    return shareable->reference_count++;
}

uacpi_u32 uacpi_shareable_unref(uacpi_handle handle)
{
    struct uacpi_shareable *shareable = handle;

    if (uacpi_unlikely(uacpi_bugged_shareable(shareable)))
        return BUGGED_REFCOUNT;

    return shareable->reference_count--;
}

void uacpi_shareable_unref_and_delete_if_last(
    uacpi_handle handle, void (*do_free)(uacpi_handle)
)
{
    if (handle == UACPI_NULL)
        return;

    if (uacpi_unlikely(uacpi_bugged_shareable(handle)))
        return;

    if (uacpi_shareable_unref(handle) == 1)
        do_free(handle);
}

uacpi_u32 uacpi_shareable_refcount(uacpi_handle handle)
{
    struct uacpi_shareable *shareable = handle;
    return shareable->reference_count;
}
