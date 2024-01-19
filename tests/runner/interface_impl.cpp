#include <iostream>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

#include <uacpi/kernel_api.h>

uacpi_status uacpi_kernel_io_map(uacpi_io_addr, uacpi_size,
                                 uacpi_handle *out_handle)
{
    *out_handle = nullptr;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle) {}

uacpi_status uacpi_kernel_io_read(
    uacpi_handle, uacpi_size,
    uacpi_u8 byte_width, uacpi_u64 *value
)
{
    switch (byte_width)
    {
    case 1:
        *value = 0xFF;
        break;
    case 2:
        *value = 0xFFFF;
        break;
    case 4:
        *value = 0xFFFFFFFF;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read(
    uacpi_pci_address*, uacpi_size offset,
    uacpi_u8 byte_width, uacpi_u64 *value
)
{
    return uacpi_kernel_io_read(nullptr, offset, byte_width, value);
}

uacpi_status uacpi_kernel_io_write(
    uacpi_handle, uacpi_size,
    uacpi_u8 byte_width, uacpi_u64
)
{
    switch (byte_width) {
    case 1:
    case 2:
    case 4:
        return UACPI_STATUS_OK;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
}

uacpi_status uacpi_kernel_pci_write(
    uacpi_pci_address*, uacpi_size, uacpi_u8, uacpi_u64
)
{
    return UACPI_STATUS_OK;
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size)
{
    return reinterpret_cast<void*>(addr);
}

void uacpi_kernel_unmap(void*, uacpi_size) {}

void* uacpi_kernel_alloc(uacpi_size size) { return malloc(size); }
void uacpi_kernel_free(void* mem) { free(mem); }

void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size)
{
    return calloc(count, size);
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

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
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

class Event {
public:
    void signal()
    {
        std::unique_lock<std::mutex> lock(mutex);
        counter++;
        cv.notify_one();
    }

    void reset()
    {
        std::unique_lock<std::mutex> lock(mutex);
        counter = 0;
    }

    uacpi_bool wait(uacpi_u16 timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);

        if (counter) {
            counter--;
            return UACPI_TRUE;
        }

        if (timeout == 0)
            return UACPI_FALSE;

        if (timeout == 0xFFFF) {
            cv.wait(lock, [this] {
                return counter != 0;
            });
            counter--;
            return UACPI_TRUE;
        }

        auto wait_res = cv.wait_for(
            lock, std::chrono::milliseconds(timeout),
            [this] { return counter != 0; }
        );
        if (!wait_res)
            return UACPI_FALSE;

        counter--;
        return UACPI_TRUE;
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    uacpi_size counter = 0;
};

uacpi_handle uacpi_kernel_create_event(void)
{
    return new Event;
}

void uacpi_kernel_free_event(uacpi_handle handle)
{
    auto *event = (Event*)handle;
    delete event;
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout)
{
    auto *event = (Event*)handle;
    return event->wait(timeout);
}

void uacpi_kernel_signal_event(uacpi_handle handle)
{
    auto *event = (Event*)handle;
    return event->signal();
}

void uacpi_kernel_reset_event(uacpi_handle handle)
{
    auto *event = (Event*)handle;
    return event->reset();
}
