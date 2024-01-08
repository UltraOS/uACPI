#pragma once

#include <uacpi/internal/compiler.h>
#include <uacpi/internal/helpers.h>
#include <uacpi/types.h>

/*
 * -----------------------------------------------------
 * Common structures provided by the ACPI specification
 * -----------------------------------------------------
 */

#define ACPI_RSDP_SIGNATURE "RSD PTR "
#define ACPI_RSDT_SIGNATURE "RSDT"
#define ACPI_XSDT_SIGNATURE "XSDT"
#define ACPI_FADT_SIGNATURE "FACP"
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
    uacpi_u8 address_space_id;
    uacpi_u8 register_bit_width;
    uacpi_u8 register_bit_offset;
    uacpi_u8 access_size;
    uacpi_u64 address;
})
UACPI_EXPECT_SIZEOF(struct acpi_gas, 12);

UACPI_PACKED(struct acpi_rsdp {
    char signature[8];
    uacpi_u8 checksum;
    char oemid[6];
    uacpi_u8 revision;
    uacpi_u32 rsdt_addr;

    // vvvv available if .revision >= 2.0 only
    uacpi_u32 length;
    uacpi_u64 xsdt_addr;
    uacpi_u8 extended_checksum;
    uacpi_u8 rsvd[3];
})
UACPI_EXPECT_SIZEOF(struct acpi_rsdp, 36);

struct acpi_sdt_hdr {
    char signature[4];
    uacpi_u32 length;
    uacpi_u8 revision;
    uacpi_u8 checksum;
    char oemid[6];
    char oem_table_id[8];
    uacpi_u32 oem_revision;
    uacpi_u32 creator_id;
    uacpi_u32 creator_revision;
};
UACPI_EXPECT_SIZEOF(struct acpi_sdt_hdr, 36);

struct acpi_rsdt {
    struct acpi_sdt_hdr hdr;
    uacpi_u32 entries[];
};

struct acpi_xsdt {
    struct acpi_sdt_hdr hdr;
    uacpi_u64 entries[];
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
    uacpi_u32 firmware_ctrl;
    uacpi_u32 dsdt;
    uacpi_u8 int_model;
    uacpi_u8 preferred_pm_profile;
    uacpi_u16 sci_int;
    uacpi_u32 smi_cmd;
    uacpi_u8 acpi_enable;
    uacpi_u8 acpi_disable;
    uacpi_u8 s4bios_req;
    uacpi_u8 pstate_cnt;
    uacpi_u32 pm1a_evt_blk;
    uacpi_u32 pm1b_evt_blk;
    uacpi_u32 pm1a_cnt_blk;
    uacpi_u32 pm1b_cnt_blk;
    uacpi_u32 pm2_cnt_blk;
    uacpi_u32 pm_tmr_blk;
    uacpi_u32 gpe0_blk;
    uacpi_u32 gpe1_blk;
    uacpi_u8 pm1_evt_len;
    uacpi_u8 pm1_cnt_len;
    uacpi_u8 pm2_cnt_len;
    uacpi_u8 pm_tmr_len;
    uacpi_u8 gpe0_blk_len;
    uacpi_u8 gpe1_blk_len;
    uacpi_u8 gpe1_base;
    uacpi_u8 cst_cnt;
    uacpi_u16 p_lvl2_lat;
    uacpi_u16 p_lvl3_lat;
    uacpi_u16 flush_size;
    uacpi_u16 flush_stride;
    uacpi_u8 duty_offset;
    uacpi_u8 duty_width;
    uacpi_u8 day_alrm;
    uacpi_u8 mon_alrm;
    uacpi_u8 century;
    uacpi_u16 iapc_boot_arch;
    uacpi_u8 rsvd;
    uacpi_u32 flags;
    struct acpi_gas reset_reg;
    uacpi_u8 reset_value;
    uacpi_u16 arm_boot_arch;
    uacpi_u8 fadt_minor_verison;
    uacpi_u64 x_firmware_ctrl;
    uacpi_u64 x_dsdt;
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
    uacpi_u64 hypervisor_vendor_identity;
})
UACPI_EXPECT_SIZEOF(struct acpi_fadt, 276);

// acpi_facs->flags
#define ACPI_S4BIOS_F               (1 << 0)
#define ACPI_64BIT_WAKE_SUPPORTED_F (1 << 1)

// acpi_facs->ospm_flags
#define ACPI_64BIT_WAKE_F           (1 << 0)

UACPI_PACKED(struct acpi_facs {
    char signature[4];
    uacpi_u32 length;
    uacpi_u32 hardware_signature;
    uacpi_u32 firmware_waking_vector;
    uacpi_u32 global_lock;
    uacpi_u32 flags;
    uacpi_u64 x_firmware_waking_vector;
    uacpi_u8 version;
    char rsvd0[3];
    uacpi_u32 ospm_flags;
    char rsvd1[24];
})
UACPI_EXPECT_SIZEOF(struct acpi_facs, 64);

UACPI_PACKED(struct acpi_dsdt {
    struct acpi_sdt_hdr hdr;
    uacpi_u8 definition_block[];
})

UACPI_PACKED(struct acpi_ssdt {
    struct acpi_sdt_hdr hdr;
    uacpi_u8 definition_block[];
})

/*
 * ACPI 6.5 specification:
 * Bit [0] - Set if the device is present.
 * Bit [1] - Set if the device is enabled and decoding its resources.
 * Bit [2] - Set if the device should be shown in the UI.
 * Bit [3] - Set if the device is functioning properly (cleared if device
 *           failed its diagnostics).
 * Bit [4] - Set if the battery is present.
 */
#define ACPI_STA_RESULT_DEVICE_PRESENT (1 << 0)
#define ACPI_STA_RESULT_DEVICE_ENABLED (1 << 1)
#define ACPI_STA_RESULT_DEVICE_SHOWN_IN_UI (1 << 2)
#define ACPI_STA_RESULT_DEVICE_FUNCTIONING (1 << 3)
#define ACPI_STA_RESULT_DEVICE_BATTERY_PRESENT (1 << 4)
