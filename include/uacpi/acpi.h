#pragma once

#include <stdint.h>
#include <uacpi/internal/attributes.h>
#include <uacpi/internal/helpers.h>

/*
 * -----------------------------------------------------
 * Common structures provided by the ACPI specification
 * -----------------------------------------------------
 */

#define ACPI_RSDP_SIGNATURE "RSD PTR "
#define ACPI_RSDT_SIGNATURE "RSDT"
#define ACPI_XSDT_SIGNATURE "XSDT"
#define ACPI_FADT_SIGNATURE "FADT"
#define ACPI_FACS_SIGNATURE "FACS"
#define ACPI_DSDT_SIGNATURE "DSDT"
#define ACPI_SSDT_SIGNATURE "SSDT"

#define ACPI_AS_ID_SYS_MEM       0x00
#define ACPI_AS_ID_SYS_IO        0x01
#define ACPI_AS_ID_PCI_CFG_SPACE 0x02
#define ACPI_AS_ID_EC            0x03
#define ACPI_AS_ID_SMBUS         0x04
#define ACPI_AS_ID_SYS_CMOS      0x05
#define ACPI_AS_ID_PCI_BAR_TGT   0x06
#define ACPI_AS_ID_IPMI          0x07
#define ACPI_AS_ID_GP_IO         0x08
#define ACPI_AS_ID_GENERIC_SBUS  0x09
#define ACPI_AS_ID_PCC           0x0A
#define ACPI_AS_ID_FFH           0x7F
#define ACPI_AS_ID_OEM_BASE      0xC0
#define ACPI_AS_ID_OEM_END       0xFF

#define ACPI_ACCESS_UD    0
#define ACPI_ACCESS_BYTE  1
#define ACPI_ACCESS_WORD  2
#define ACPI_ACCESS_DWORD 3
#define ACPI_ACCESS_QWORD 4

UACPI_PACKED(struct acpi_gas {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
});
UACPI_EXPECT_SIZEOF(struct acpi_gas, 12);

UACPI_PACKED(struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_addr;

    // vvvv available if .revision >= 2.0 only
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t extended_checksum;
    uint8_t rsvd[3];
});
UACPI_EXPECT_SIZEOF(struct acpi_rsdp, 36);

struct acpi_sdt_hdr {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};
UACPI_EXPECT_SIZEOF(struct acpi_sdt_hdr, 36);

struct acpi_rsdt {
    struct acpi_sdt_hdr hdr;
    uint32_t entries[];
};

struct acpi_xsdt {
    struct acpi_sdt_hdr hdr;
    uint64_t entries[];
};

// acpi_fdt->iapc_flags
#define ACPI_IA_PC_LEGACY_DEVS  (1 << 0)
#define ACPI_IA_PC_8042         (1 << 1)
#define ACPI_IA_PC_NO_VGA       (1 << 2)
#define ACPI_IA_PC_NO_MSI       (1 << 3)
#define ACPI_IA_PC_NO_PCIE_ASPM (1 << 4)
#define ACPI_IA_PC_NO_CMOS_RTC  (1 << 5)

// acpi_fdt->flags
#define ACPI_WBINVD                    (1 << 0)
#define ACPI_WBINVD_FLUSH              (1 << 1)
#define ACPI_PROC_C1                   (1 << 2)
#define ACPI_P_LVL2_UP                 (1 << 3)
#define ACPI_PWR_BUTTON                (1 << 4)
#define ACPI_SLP_BUTTON                (1 << 5)
#define ACPI_FIX_RTC                   (1 << 6)
#define ACPI_RTC_S4                    (1 << 7)
#define ACPI_TMR_VAL_EXT               (1 << 8)
#define ACPI_DCK_CAP                   (1 << 9)
#define ACPI_RESET_REG_SUP             (1 << 10)
#define ACPI_SEALED_CASE               (1 << 11)
#define ACPI_HEADLESS                  (1 << 12)
#define ACPI_CPU_SW_SLP                (1 << 13)
#define ACPI_PCI_EXP_WAK               (1 << 14)
#define ACPI_USE_PLATFORM_CLOCK        (1 << 15)
#define ACPI_S4_RTC_STS_VALID          (1 << 16)
#define ACPI_REMOTE_POWER_ON_CAPABLE   (1 << 17)
#define ACPI_FORCE_APIC_CLUSTER_MODEL  (1 << 18)
#define ACPI_FORCE_APIC_PHYS_DEST_MODE (1 << 19)
#define ACPI_HW_REDUCED_ACPI           (1 << 20)
#define ACPI_LOW_POWER_S0_IDLE_CAPABLE (1 << 21)

// acpi_fdt->arm_flags
#define ACPI_ARM_PSCI_COMPLIANT (1 << 0)
#define ACPI_ARM_PSCI_USE_HVC   (1 << 1)

UACPI_PACKED(struct acpi_fadt {
    struct acpi_sdt_hdr hdr;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t int_model;
    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t pm1_evt_len;
    uint8_t pm1_cnt_len;
    uint8_t pm2_cnt_len;
    uint8_t pm_tmr_len;
    uint8_t gpe0_blk_len;
    uint8_t gpe1_blk_len;
    uint8_t gpe1_base;
    uint8_t cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
    uint16_t iapc_boot_arch;
    uint8_t rsvd;
    uint32_t flags;
    struct acpi_gas reset_reg;
    uint8_t reset_value;
    uint16_t arm_boot_arch;
    uint8_t fadt_minor_verison;
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    struct acpi_gas x_pm1a_evt_blk;
    struct acpi_gas x_pm1b_evt_blk;
    struct acpi_gas x_pm1a_cnt_blk;
    struct acpi_gas x_pm1b_cnt_blk;
    struct acpi_gas x_pm2_cnt_blk;
    struct acpi_gas x_pm_tmr_blk;
    struct acpi_gas x_gpe0_blk;
    struct acpi_gas x_gpe1_blk;
    struct acpi_gas sleep_control_reg;
    struct acpi_gas sleep_status_reg;
    uint64_t hypervisor_vendor_identity;
});
UACPI_EXPECT_SIZEOF(struct acpi_fadt, 276);

// acpi_facs->flags
#define ACPI_S4BIOS_F               (1 << 0)
#define ACPI_64BIT_WAKE_SUPPORTED_F (1 << 1)

// acpi_facs->ospm_flags
#define ACPI_64BIT_WAKE_F           (1 << 0)

UACPI_PACKED(struct acpi_facs {
    char signature[4];
    uint32_t length;
    uint32_t hardware_signature;
    uint32_t firmware_waking_vector;
    uint32_t global_lock;
    uint32_t flags;
    uint64_t x_firmware_waking_vector;
    uint8_t version;
    char rsvd0[3];
    uint32_t ospm_flags;
    char rsvd1[24];
});
UACPI_EXPECT_SIZEOF(struct acpi_facs, 64);

UACPI_PACKED(struct acpi_dsdt {
    struct acpi_sdt_hdr hdr;
    uint8_t definition_block[];
});

UACPI_PACKED(struct acpi_ssdt {
    struct acpi_sdt_hdr hdr;
    uint8_t definition_block[];
});
