#include <uacpi/internal/mutex.h>
#include <uacpi/internal/log.h>
#include <uacpi/kernel_api.h>

uacpi_bool uacpi_this_thread_owns_aml_mutex(uacpi_mutex *mutex)
{
    uacpi_thread_id id;

    id = UACPI_ATOMIC_LOAD_THREAD_ID(&mutex->owner);
    return id == uacpi_kernel_get_thread_id();
}

uacpi_bool uacpi_acquire_aml_mutex(uacpi_mutex *mutex, uacpi_u16 timeout)
{
    uacpi_thread_id this_id;

    this_id = uacpi_kernel_get_thread_id();
    if (UACPI_ATOMIC_LOAD_THREAD_ID(&mutex->owner) == this_id) {
        if (uacpi_unlikely(mutex->depth == 0xFFFF)) {
            uacpi_warn(
                "failing an attempt to acquire mutex @%p, too many recursive "
                "acquires\n", mutex
            );
            return UACPI_FALSE;
        }

        mutex->depth++;
        return UACPI_TRUE;
    }

    if (!uacpi_kernel_acquire_mutex(mutex->handle, timeout))
        return UACPI_FALSE;

    UACPI_ATOMIC_STORE_THREAD_ID(&mutex->owner, this_id);
    mutex->depth = 1;
    return UACPI_TRUE;
}

void uacpi_release_aml_mutex(uacpi_mutex *mutex)
{
    if (mutex->depth-- > 1)
        return;

    UACPI_ATOMIC_STORE_THREAD_ID(&mutex->owner, UACPI_THREAD_ID_NONE);
    uacpi_kernel_release_mutex(mutex->handle);
}
