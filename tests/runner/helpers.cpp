#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <filesystem>
#include <fstream>

#include "helpers.h"

void build_xsdt_from_file(full_xsdt& xsdt, acpi_rsdp& rsdp,
                          std::string_view path)
{
    auto& fadt = *new acpi_fadt {};

    auto* dsdt = reinterpret_cast<acpi_dsdt*>(read_entire_file(path));
    auto* hdr = &fadt.hdr;
    fadt.x_dsdt = reinterpret_cast<uacpi_phys_addr>(dsdt);
    memcpy(hdr->signature, ACPI_FADT_SIGNATURE,
           sizeof(ACPI_FADT_SIGNATURE) - 1);

    xsdt.fadt = &fadt;
    hdr = &xsdt.hdr;

    hdr->length = sizeof(xsdt);
    hdr->revision = dsdt->hdr.revision;
    memcpy(hdr->oemid, dsdt->hdr.oemid, sizeof(dsdt->hdr.oemid));
    hdr->oem_revision = dsdt->hdr.oem_revision;

    if (sizeof(void*) == 4) {
        memcpy(hdr->signature, ACPI_RSDT_SIGNATURE,
               sizeof(ACPI_XSDT_SIGNATURE) - 1);

        rsdp.rsdt_addr = reinterpret_cast<size_t>(&xsdt);
    } else {
        memcpy(hdr->signature, ACPI_XSDT_SIGNATURE,
               sizeof(ACPI_XSDT_SIGNATURE) - 1);

        rsdp.xsdt_addr = reinterpret_cast<size_t>(&xsdt);
        rsdp.length = sizeof(rsdp);
        rsdp.revision = 1;
    }
}

void* read_entire_file(std::string_view path)
{
    size_t file_size = std::filesystem::file_size(path);
    std::ifstream file(path.data());

    if (!file)
        throw std::runtime_error(
            std::string("failed to open file ") + path.data()
        );

    auto* buf = new uint8_t[file_size];
    file.read(reinterpret_cast<char*>(buf), file_size);

    if (!file) {
        delete[] buf;
        throw std::runtime_error(
            std::string("failed to read entire file ") + path.data()
        );
    }

    return buf;
}
