#include <iostream>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <cinttypes>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#else
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#endif

#include <uacpi/kernel_api.h>

uacpi_phys_addr g_rsdp;

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rdsp_address)
{
    *out_rdsp_address = g_rsdp;
    return UACPI_STATUS_OK;
}

#ifdef UACPI_KERNEL_INITIALIZATION
uacpi_status uacpi_kernel_initialize(uacpi_init_level)
{
    return UACPI_STATUS_OK;
}
void uacpi_kernel_deinitialize(void) { }
#endif

uacpi_status uacpi_kernel_raw_memory_read(
    uacpi_phys_addr, uacpi_u8, uacpi_u64 *ret
)
{
    *ret = 0;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_write(
    uacpi_phys_addr, uacpi_u8, uacpi_u64
)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_read(
    uacpi_io_addr, uacpi_u8, uacpi_u64 *ret
)
{
    *ret = 0xFFFFFFFFFFFFFFFF;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_write(
    uacpi_io_addr, uacpi_u8, uacpi_u64
)
{
    return UACPI_STATUS_OK;
}

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
        break;
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

bool g_expect_virtual_addresses = true;

struct mapping {
    void *virt;
    uacpi_size size;

    bool operator==(const mapping& m) const
    {
        return size == m.size;
    }
};

template <>
struct std::hash<mapping>
{
    size_t operator()(const mapping& m) const
    {
        return m.size;
    }
};

static
std::unordered_map<void*, std::pair<uacpi_phys_addr, size_t>>
virt_to_phys_and_refcount;

static std::unordered_map<uacpi_phys_addr, std::unordered_set<mapping>>
phys_to_virt;

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size size)
{
    if (!g_expect_virtual_addresses) {
        auto it = phys_to_virt.find(addr);
        if (it != phys_to_virt.end()) {
            auto mapping_it = it->second.find({ nullptr, size });

            if (mapping_it != it->second.end()) {
                virt_to_phys_and_refcount[mapping_it->virt].second++;
                return mapping_it->virt;
            }

            std::printf("WARN: remapping physical 0x%016" PRIX64
                        " with size %zu\n", addr, size);
        }

        void *virt = std::calloc(size, 1);
        mapping m = { virt, size };

        virt_to_phys_and_refcount[virt] = { addr, 1 };
        phys_to_virt[addr].insert(m);
        return virt;
    }

    return reinterpret_cast<void*>(addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size size)
{
    auto it = virt_to_phys_and_refcount.find(addr);
    if (it == virt_to_phys_and_refcount.end())
        return;

    if (--it->second.second > 0)
        return;

    auto phys_it = phys_to_virt.find(it->second.first);
    auto mapping_it = phys_it->second.find({ nullptr, size });
    if (mapping_it == phys_it->second.end()) {
        std::printf("WARN: cannot identify mapping virt=%p phys=0x%016" PRIX64
                    " with size %zu\n", addr, phys_it->first, size);
        return;
    }

    phys_it->second.erase(mapping_it);
    if (phys_it->second.empty())
        phys_to_virt.erase(it->second.first);

    std::free(it->first);
    virt_to_phys_and_refcount.erase(it);
}

#ifdef UACPI_SIZED_FREES
static std::unordered_map<void*, uacpi_size> allocations;

void* uacpi_kernel_alloc(uacpi_size size)
{
    auto *ret = malloc(size);
    if (ret == nullptr)
        return ret;

    allocations[ret] = size;
    return ret;
}

void uacpi_kernel_free(void* mem, uacpi_size size)
{
    if (mem == nullptr)
        return;

    auto it = allocations.find(mem);
    if (it == allocations.end()) {
        std::fprintf(stderr, "unable to find heap allocation %p\n", mem);
        std::abort();
    }

    if (it->second != size) {
        std::fprintf(
            stderr,
            "invalid free size: originally allocated %zu bytes, "
            "freeing as %zu\n", it->second, size
        );
        std::abort();
    }

    allocations.erase(it);
    free(mem);
}

void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size)
{
    auto *ret = uacpi_kernel_alloc(count * size);
    if (ret == nullptr)
        return ret;

    memset(ret, 0, count * size);
    return ret;
}
#else
void* uacpi_kernel_alloc(uacpi_size size)
{
    return malloc(size);
}

void uacpi_kernel_free(void* mem)
{
    return free(mem);
}

void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size)
{
    return calloc(count, size);
}
#endif

static const char* log_level_to_string(uacpi_log_level lvl)
{
    switch (lvl) {
    case UACPI_LOG_DEBUG:
        return "DEBUG";
    case UACPI_LOG_TRACE:
        return "TRACE";
    case UACPI_LOG_INFO:
        return "INFO";
    case UACPI_LOG_WARN:
        return "WARN";
    case UACPI_LOG_ERROR:
        return "ERROR";
    default:
        std::abort();
    }
}

#ifdef UACPI_FORMATTED_LOGGING
void uacpi_kernel_vlog(enum uacpi_log_level lvl, const char* text, uacpi_va_list vlist)
{
    printf("[uACPI][%s] ", log_level_to_string(lvl));
    vprintf(text, vlist);
}

void uacpi_kernel_log(enum uacpi_log_level lvl, const char* text, ...)
{
    va_list vlist;
    va_start(vlist, text);

    uacpi_kernel_vlog(lvl, text, vlist);

    va_end(vlist);
}
#else
void uacpi_kernel_log(enum uacpi_log_level lvl, const char* text)
{
    printf("[uACPI][%s] %s", log_level_to_string(lvl), text);
}
#endif


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

    return (ts.tv_nsec + ts.tv_sec * 1000000000ull) / 100;
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

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
#ifdef _WIN32
    return (uacpi_thread_id)((uintptr_t)GetCurrentThreadId());
#elif defined(__APPLE__)
    uint64_t id;

    if (pthread_threadid_np(NULL, &id) < 0) {
        std::cout << "pthread_threadid_np failed" << std::endl;
        std::exit(1);
    }

    return (uacpi_thread_id)id;
#else
    return (uacpi_thread_id)((unsigned long)gettid());
#endif
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

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* req)
{
    switch (req->type) {
    case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT:
        std::cout << "Ignoring breakpoint" << std::endl;
        break;
    case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
        std::cout << "Fatal firmware error:"
                  << " type: " << std::hex << (int)req->fatal.type
                  << " code: " << std::hex << req->fatal.code
                  << " arg: " << std::hex << req->fatal.arg << std::endl;
        break;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32, uacpi_interrupt_handler, uacpi_handle,
    uacpi_handle*
)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle
)
{
    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    return uacpi_kernel_create_mutex();
}

void uacpi_kernel_free_spinlock(uacpi_handle handle)
{
    return uacpi_kernel_free_mutex(handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle)
{
    uacpi_kernel_acquire_mutex(handle, 0xFFFF);
    return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags)
{
    uacpi_kernel_release_mutex(handle);
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler handler, uacpi_handle ctx
)
{
    handler(ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion()
{
    return UACPI_STATUS_OK;
}
