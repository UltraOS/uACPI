#include <string_view>

#include <uacpi/acpi.h>
#include <uacpi/uacpi.h>

UACPI_PACKED(struct full_xsdt {
    struct acpi_sdt_hdr hdr;
    acpi_fadt* fadt;
})

void build_xsdt_from_file(full_xsdt& xsdt, acpi_rsdp& rsdp,
                          std::string_view path);

void* read_entire_file(std::string_view path, size_t min_size = 0);
