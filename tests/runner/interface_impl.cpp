#include <iostream>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

#include <uacpi/kernel_api.h>

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    (void)len;
    return reinterpret_cast<void*>(addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len)
{
    (void)addr;
    (void)len;
}

void* uacpi_kernel_alloc(uacpi_size size)
{
    return malloc(size);
}

void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size)
{
    return calloc(count, size);
}

void uacpi_kernel_free(void* mem)
{
    free(mem);
}

void uacpi_kernel_vlog(enum uacpi_log_level lvl, const char* text, uacpi_va_list vlist)
{
    const char *lvl_str;

    switch (lvl) {
    case UACPI_LOG_TRACE:
        lvl_str = "TRACE";
        break;
    case UACPI_LOG_INFO:
        lvl_str = "INFO";
        break;
    case UACPI_LOG_WARN:
        lvl_str = "WARN";
        break;
    case UACPI_LOG_ERROR:
        lvl_str = "ERROR";
        break;
    case UACPI_LOG_FATAL:
        lvl_str = "FATAL";
        break;
    default:
        std::abort();
    }

    printf("[uACPI][%s] ", lvl_str);
    vprintf(text, vlist);
}

void uacpi_kernel_log(enum uacpi_log_level lvl, const char* text, ...)
{
    va_list vlist;
    va_start(vlist, text);

    uacpi_kernel_vlog(lvl, text, vlist);

    va_end(vlist);
}

uacpi_u64 uacpi_kernel_get_ticks(void)
{
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if (frequency.QuadPart == 0) {
        if (!QueryPerformanceFrequency(&frequency)) {
            puts("QueryPerformanceFrequency() returned an error");
            std::abort();
        }
    }

    if (!QueryPerformanceCounter(&counter)) {
        puts("QueryPerformanceCounter() returned an error");
        std::abort();
    }

    // Convert to 100 nanoseconds
    counter.QuadPart *= 10000000;
    return counter.QuadPart / frequency.QuadPart;
#elif defined(__APPLE__)
    static struct mach_timebase_info tb;
    static bool initialized;
    uacpi_u64 nanoseconds;

    if (!initialized) {
        if (mach_timebase_info(&tb) != KERN_SUCCESS) {
            puts("mach_timebase_info() returned an error");
            std::abort();
        }
        initialized = true;
    }

    nanoseconds = (mach_absolute_time() * tb.numer) / tb.denom;
    return nanoseconds / 100;
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        puts("clock_gettime() returned an error");
        std::abort();
    }

    return (ts.tv_nsec + ts.tv_sec * 1000000000) / 100;
#endif
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

uacpi_handle uacpi_kernel_create_mutex(void)
{
    return new std::timed_mutex();
}

void uacpi_kernel_free_mutex(uacpi_handle handle)
{
    auto* mutex = (std::timed_mutex*)handle;
    delete mutex;
}

uacpi_bool uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout)
{
    auto *mutex = (std::timed_mutex*)handle;

    if (timeout == 0)
        return mutex->try_lock();

    if (timeout == 0xFFFF) {
        mutex->lock();
        return UACPI_TRUE;
    }

    return mutex->try_lock_for(std::chrono::milliseconds(timeout));
}

void uacpi_kernel_release_mutex(uacpi_handle handle)
{
    auto *mutex = (std::timed_mutex*)handle;

    mutex->unlock();
}
