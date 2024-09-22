#include <uacpi/platform/atomic.h>
#include <uacpi/internal/mutex.h>
#include <uacpi/internal/log.h>
#include <uacpi/internal/registers.h>
#include <uacpi/internal/context.h>
#include <uacpi/kernel_api.h>

#if UACPI_REDUCED_HARDWARE == 0

#define GLOBAL_LOCK_PENDING (1 << 0)

#define GLOBAL_LOCK_OWNED_BIT 1
#define GLOBAL_LOCK_OWNED (1 << GLOBAL_LOCK_OWNED_BIT)

#define GLOBAL_LOCK_MASK 0b11u

static uacpi_bool try_acquire_global_lock_from_firmware(uacpi_u32 *lock)
{
    uacpi_u32 value, new_value;
    uacpi_bool was_owned;

    value = *(volatile uacpi_u32*)lock;
    do {
        was_owned = (value & GLOBAL_LOCK_OWNED) >> GLOBAL_LOCK_OWNED_BIT;

        // Clear both owned & pending bits.
        new_value = value & ~GLOBAL_LOCK_MASK;

        // Set owned unconditionally
        new_value |= GLOBAL_LOCK_OWNED;

        // Set pending iff the lock was owned at the time of reading
        if (was_owned)
            new_value |= GLOBAL_LOCK_PENDING;
    } while (!uacpi_atomic_cmpxchg32(lock, &value, new_value));

    return !was_owned;
}

static uacpi_bool do_release_global_lock_to_firmware(uacpi_u32 *lock)
{
    uacpi_u32 value, new_value;

    value = *(volatile uacpi_u32*)lock;
    do {
        new_value = value & ~GLOBAL_LOCK_MASK;
    } while (!uacpi_atomic_cmpxchg32(lock, &value, new_value));

    return value & GLOBAL_LOCK_PENDING;
}

static uacpi_status uacpi_acquire_global_lock_from_firmware(void)
{
    uacpi_cpu_flags flags;
    uacpi_u16 spins = 0;
    uacpi_bool success;

    if (!g_uacpi_rt_ctx.has_global_lock)
        return UACPI_STATUS_OK;

    flags = uacpi_kernel_lock_spinlock(g_uacpi_rt_ctx.global_lock_spinlock);
    for (;;) {
        spins++;
        uacpi_trace(
            "trying to acquire the global lock from firmware... (attempt %u)\n",
            spins
        );

        success = try_acquire_global_lock_from_firmware(
            &g_uacpi_rt_ctx.facs->global_lock
        );
        if (success)
            break;

        if (uacpi_unlikely(spins == 0xFFFF))
            break;

        g_uacpi_rt_ctx.global_lock_pending = UACPI_TRUE;
        uacpi_trace(
            "global lock is owned by firmware, waiting for a release "
            "notification...\n"
        );
        uacpi_kernel_unlock_spinlock(g_uacpi_rt_ctx.global_lock_spinlock, flags);

        uacpi_kernel_wait_for_event(g_uacpi_rt_ctx.global_lock_event, 0xFFFF);
        flags = uacpi_kernel_lock_spinlock(g_uacpi_rt_ctx.global_lock_spinlock);
    }

    g_uacpi_rt_ctx.global_lock_pending = UACPI_FALSE;
    uacpi_kernel_unlock_spinlock(g_uacpi_rt_ctx.global_lock_spinlock, flags);

    if (uacpi_unlikely(!success)) {
        uacpi_error("unable to acquire global lock after %u attempts\n", spins);
        return UACPI_STATUS_HARDWARE_TIMEOUT;
    }

    uacpi_trace("global lock successfully acquired after %u attempt%s\n",
                spins, spins > 1 ? "s" : "");
    return UACPI_STATUS_OK;
}

static void uacpi_release_global_lock_to_firmware(void)
{
    if (!g_uacpi_rt_ctx.has_global_lock)
        return;

    uacpi_trace("releasing the global lock to firmware...\n");
    if (do_release_global_lock_to_firmware(&g_uacpi_rt_ctx.facs->global_lock)) {
        uacpi_trace("notifying firmware of the global lock release since the "
                    "pending bit was set\n");
        uacpi_write_register_field(UACPI_REGISTER_FIELD_GBL_RLS, 1);
    }
}
#endif

UACPI_ALWAYS_OK_FOR_REDUCED_HARDWARE(
    uacpi_status uacpi_acquire_global_lock_from_firmware(void)
)
UACPI_STUB_IF_REDUCED_HARDWARE(
    void uacpi_release_global_lock_to_firmware(void)
)

uacpi_status uacpi_acquire_global_lock(uacpi_u16 timeout, uacpi_u32 *out_seq)
{
    uacpi_bool did_acquire;
    uacpi_status ret;

    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED);

    if (uacpi_unlikely(out_seq == UACPI_NULL))
        return UACPI_STATUS_INVALID_ARGUMENT;

    UACPI_MUTEX_ACQUIRE_WITH_TIMEOUT(
        g_uacpi_rt_ctx.global_lock_mutex, timeout, did_acquire
    );
    if (!did_acquire)
        return UACPI_STATUS_TIMEOUT;

    ret = uacpi_acquire_global_lock_from_firmware();
    if (uacpi_unlikely_error(ret)) {
        UACPI_MUTEX_RELEASE(g_uacpi_rt_ctx.global_lock_mutex);
        return ret;
    }

    if (uacpi_unlikely(g_uacpi_rt_ctx.global_lock_seq_num == 0xFFFFFFFF))
        g_uacpi_rt_ctx.global_lock_seq_num = 0;

    *out_seq = g_uacpi_rt_ctx.global_lock_seq_num++;
    g_uacpi_rt_ctx.global_lock_acquired = UACPI_TRUE;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_release_global_lock(uacpi_u32 seq)
{
    UACPI_ENSURE_INIT_LEVEL_AT_LEAST(UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED);

    if (uacpi_unlikely(!g_uacpi_rt_ctx.global_lock_acquired ||
                       seq != g_uacpi_rt_ctx.global_lock_seq_num))
        return UACPI_STATUS_INVALID_ARGUMENT;

    g_uacpi_rt_ctx.global_lock_acquired = UACPI_FALSE;
    uacpi_release_global_lock_to_firmware();
    UACPI_MUTEX_RELEASE(g_uacpi_rt_ctx.global_lock_mutex);

    return UACPI_STATUS_OK;
}

uacpi_bool uacpi_this_thread_owns_aml_mutex(uacpi_mutex *mutex)
{
    uacpi_thread_id id;

    id = UACPI_ATOMIC_LOAD_THREAD_ID(&mutex->owner);
    return id == uacpi_kernel_get_thread_id();
}

uacpi_bool uacpi_acquire_aml_mutex(uacpi_mutex *mutex, uacpi_u16 timeout)
{
    uacpi_thread_id this_id;
    uacpi_bool did_acquire;

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

    UACPI_MUTEX_ACQUIRE_WITH_TIMEOUT(mutex->handle, timeout, did_acquire);
    if (!did_acquire)
        return UACPI_FALSE;

    if (mutex->handle == g_uacpi_rt_ctx.global_lock_mutex) {
        uacpi_status ret;

        ret = uacpi_acquire_global_lock_from_firmware();
        if (uacpi_unlikely_error(ret)) {
            UACPI_MUTEX_RELEASE(mutex->handle);
            return UACPI_FALSE;
        }
    }

    UACPI_ATOMIC_STORE_THREAD_ID(&mutex->owner, this_id);
    mutex->depth = 1;
    return UACPI_TRUE;
}

void uacpi_release_aml_mutex(uacpi_mutex *mutex)
{
    if (mutex->depth-- > 1)
        return;

    if (mutex->handle == g_uacpi_rt_ctx.global_lock_mutex)
        uacpi_release_global_lock_to_firmware();

    UACPI_ATOMIC_STORE_THREAD_ID(&mutex->owner, UACPI_THREAD_ID_NONE);
    UACPI_MUTEX_RELEASE(mutex->handle);
}
