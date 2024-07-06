#include <string_view>

#include <uacpi/acpi.h>
#include <uacpi/uacpi.h>

template <typename ExprT>
class ScopeGuard
{
public:
    ScopeGuard(ExprT expr)
            : callback(std::move(expr)) {}

    ~ScopeGuard() { callback(); }

private:
    ExprT callback;
};

extern bool g_expect_virtual_addresses;

UACPI_PACKED(struct full_xsdt {
    struct acpi_sdt_hdr hdr;
    acpi_fadt* fadt;
})

void build_xsdt_from_file(full_xsdt& xsdt, acpi_rsdp& rsdp,
                          std::string_view path);

std::pair<void*, size_t>
read_entire_file(std::string_view path, size_t min_size = 0);
