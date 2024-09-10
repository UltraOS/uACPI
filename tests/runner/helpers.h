#include <string_view>
#include <vector>

#include <uacpi/acpi.h>
#include <uacpi/uacpi.h>

template <typename ExprT>
class ScopeGuard
{
public:
    ScopeGuard(ExprT expr)
            : callback(std::move(expr)) {}

    ~ScopeGuard() { if (!disarmed) callback(); }

    void disarm() { disarmed = true; }

private:
    ExprT callback;
    bool disarmed { false };
};

extern bool g_expect_virtual_addresses;
extern uacpi_phys_addr g_rsdp;

UACPI_PACKED(struct full_xsdt {
    struct acpi_sdt_hdr hdr;
    acpi_fadt* fadt;
    struct acpi_sdt_hdr* ssdts[];
})

void build_xsdt(full_xsdt& xsdt, acpi_rsdp& rsdp, std::string_view dsdt_path,
                const std::vector<std::string>& ssdt_paths);

std::pair<void*, size_t>
read_entire_file(std::string_view path, size_t min_size = 0);
