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
#define ACPI_PSDT_SIGNATURE "PSDT"
#define ACPI_ECDT_SIGNATURE "ECDT"

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

UACPI_PACKED(struct acpi_sdt_hdr {
    char signature[4];
    uacpi_u32 length;
    uacpi_u8 revision;
    uacpi_u8 checksum;
    char oemid[6];
    char oem_table_id[8];
    uacpi_u32 oem_revision;
    uacpi_u32 creator_id;
    uacpi_u32 creator_revision;
})
UACPI_EXPECT_SIZEOF(struct acpi_sdt_hdr, 36);

UACPI_PACKED(struct acpi_rsdt {
    struct acpi_sdt_hdr hdr;
    uacpi_u32 entries[];
})

UACPI_PACKED(struct acpi_xsdt {
    struct acpi_sdt_hdr hdr;
    uacpi_u64 entries[];
})

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

// PM1{a,b}_STS
#define ACPI_PM1_STS_TMR_STS_IDX 0
#define ACPI_PM1_STS_BM_STS_IDX 4
#define ACPI_PM1_STS_GBL_STS_IDX 5
#define ACPI_PM1_STS_PWRBTN_STS_IDX 8
#define ACPI_PM1_STS_SLPBTN_STS_IDX 9
#define ACPI_PM1_STS_RTC_STS_IDX 10
#define ACPI_PM1_STS_IGN0_IDX 11
#define ACPI_PM1_STS_PCIEXP_WAKE_STS_IDX 14
#define ACPI_PM1_STS_WAKE_STS_IDX 15

#define ACPI_PM1_STS_TMR_STS_MASK (1 << ACPI_PM1_STS_TMR_STS_IDX)
#define ACPI_PM1_STS_BM_STS_MASK (1 << ACPI_PM1_STS_BM_STS_IDX)
#define ACPI_PM1_STS_GBL_STS_MASK (1 << ACPI_PM1_STS_GBL_STS_IDX)
#define ACPI_PM1_STS_PWRBTN_STS_MASK (1 << ACPI_PM1_STS_PWRBTN_STS_IDX)
#define ACPI_PM1_STS_SLPBTN_STS_MASK (1 << ACPI_PM1_STS_SLPBTN_STS_IDX)
#define ACPI_PM1_STS_RTC_STS_MASK (1 << ACPI_PM1_STS_RTC_STS_IDX)
#define ACPI_PM1_STS_IGN0_MASK (1 << ACPI_PM1_STS_IGN0_IDX)
#define ACPI_PM1_STS_PCIEXP_WAKE_STS_MASK (1 << ACPI_PM1_STS_PCIEXP_WAKE_STS_IDX)
#define ACPI_PM1_STS_WAKE_STS_MASK (1 << ACPI_PM1_STS_WAKE_STS_IDX)

#define ACPI_PM1_STS_CLEAR 1

// PM1{a,b}_EN
#define ACPI_PM1_EN_TMR_EN_IDX 0
#define ACPI_PM1_EN_GBL_EN_IDX 5
#define ACPI_PM1_EN_PWRBTN_EN_IDX 8
#define ACPI_PM1_EN_SLPBTN_EN_IDX 9
#define ACPI_PM1_EN_RTC_EN_IDX 10
#define ACPI_PM1_EN_PCIEXP_WAKE_DIS_IDX 14

#define ACPI_PM1_EN_TMR_EN_MASK (1 << ACPI_PM1_EN_TMR_EN_IDX)
#define ACPI_PM1_EN_GBL_EN_MASK (1 << ACPI_PM1_EN_GBL_EN_IDX)
#define ACPI_PM1_EN_PWRBTN_EN_MASK (1 << ACPI_PM1_EN_PWRBTN_EN_IDX)
#define ACPI_PM1_EN_SLPBTN_EN_MASK (1 << ACPI_PM1_EN_SLPBTN_EN_IDX)
#define ACPI_PM1_EN_RTC_EN_MASK (1 << ACPI_PM1_EN_RTC_EN_IDX)
#define ACPI_PM1_EN_PCIEXP_WAKE_DIS_MASK (1 << ACPI_PM1_EN_PCIEXP_WAKE_DIS_IDX)

// PM1{a,b}_CNT_BLK
#define ACPI_PM1_CNT_SCI_EN_IDX 0
#define ACPI_PM1_CNT_BM_RLD_IDX 1
#define ACPI_PM1_CNT_GBL_RLS_IDX 2
#define ACPI_PM1_CNT_RSVD0_IDX 3
#define ACPI_PM1_CNT_RSVD1_IDX 4
#define ACPI_PM1_CNT_RSVD2_IDX 5
#define ACPI_PM1_CNT_RSVD3_IDX 6
#define ACPI_PM1_CNT_RSVD4_IDX 7
#define ACPI_PM1_CNT_RSVD5_IDX 8
#define ACPI_PM1_CNT_IGN0_IDX 9
#define ACPI_PM1_CNT_SLP_TYP_IDX 10
#define ACPI_PM1_CNT_SLP_EN_IDX 13
#define ACPI_PM1_CNT_RSVD6_IDX 14
#define ACPI_PM1_CNT_RSVD7_IDX 15

#define ACPI_SLP_TYP_MAX 0b111

#define ACPI_PM1_CNT_SCI_EN_MASK (1 << ACPI_PM1_CNT_SCI_EN_IDX)
#define ACPI_PM1_CNT_BM_RLD_MASK (1 << ACPI_PM1_CNT_BM_RLD_IDX)
#define ACPI_PM1_CNT_GBL_RLS_MASK (1 << ACPI_PM1_CNT_GBL_RLS_IDX)
#define ACPI_PM1_CNT_SLP_TYP_MASK (ACPI_SLP_TYP_MAX << ACPI_PM1_CNT_SLP_TYP_IDX)
#define ACPI_PM1_CNT_SLP_EN_MASK (1 << ACPI_PM1_CNT_SLP_EN_IDX)

/*
 * SCI_EN is not in this mask even though the spec says it must be preserved.
 * This is because it's known to be bugged on some hardware that relies on
 * software writing 1 to it after resume (as indicated by a similar comment in
 * ACPICA)
 */
#define ACPI_PM1_CNT_PRESERVE_MASK ( \
    (1 << ACPI_PM1_CNT_RSVD0_IDX) |  \
    (1 << ACPI_PM1_CNT_RSVD1_IDX) |  \
    (1 << ACPI_PM1_CNT_RSVD2_IDX) |  \
    (1 << ACPI_PM1_CNT_RSVD3_IDX) |  \
    (1 << ACPI_PM1_CNT_RSVD4_IDX) |  \
    (1 << ACPI_PM1_CNT_RSVD5_IDX) |  \
    (1 << ACPI_PM1_CNT_IGN0_IDX ) |  \
    (1 << ACPI_PM1_CNT_RSVD6_IDX) |  \
    (1 << ACPI_PM1_CNT_RSVD7_IDX)    \
)

// PM2_CNT
#define ACPI_PM2_CNT_ARB_DIS_IDX 0
#define ACPI_PM2_CNT_ARB_DIS_MASK (1 << ACPI_PM2_CNT_ARB_DIS_IDX)

// All bits are reserved but this first one
#define ACPI_PM2_CNT_PRESERVE_MASK (~((uacpi_u64)ACPI_PM2_CNT_ARB_DIS_MASK))

// SLEEP_CONTROL_REG
#define ACPI_SLP_CNT_RSVD0_IDX 0
#define ACPI_SLP_CNT_IGN0_IDX 1
#define ACPI_SLP_CNT_SLP_TYP_IDX 2
#define ACPI_SLP_CNT_SLP_EN_IDX 5
#define ACPI_SLP_CNT_RSVD1_IDX 6
#define ACPI_SLP_CNT_RSVD2_IDX 7

#define ACPI_SLP_CNT_SLP_TYP_MASK (ACPI_SLP_TYP_MAX << ACPI_SLP_CNT_SLP_TYP_IDX)
#define ACPI_SLP_CNT_SLP_EN_MASK (1 << ACPI_SLP_CNT_SLP_EN_IDX)

#define ACPI_SLP_CNT_PRESERVE_MASK ( \
    (1 << ACPI_SLP_CNT_RSVD0_IDX) |  \
    (1 << ACPI_SLP_CNT_IGN0_IDX)  |  \
    (1 << ACPI_SLP_CNT_RSVD1_IDX) |  \
    (1 << ACPI_SLP_CNT_RSVD2_IDX)    \
)

// SLEEP_STATUS_REG
#define ACPI_SLP_STS_WAK_STS_IDX 7

#define ACPI_SLP_STS_WAK_STS_MASK (1 << ACPI_SLP_STS_WAK_STS_IDX)

// All bits are reserved but this last one
#define ACPI_SLP_STS_PRESERVE_MASK (~((uacpi_u64)ACPI_SLP_STS_WAK_STS_MASK))

#define ACPI_SLP_STS_CLEAR 1

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

#define ACPI_REG_DISCONNECT 0
#define ACPI_REG_CONNECT 1

UACPI_PACKED(struct acpi_ecdt {
    struct acpi_sdt_hdr hdr;
    struct acpi_gas ec_control;
    struct acpi_gas ec_data;
    uacpi_u32 uid;
    uacpi_u8 gpe_bit;
    uacpi_char ec_id[];
})
UACPI_EXPECT_SIZEOF(struct acpi_ecdt, 65);

#define ACPI_LARGE_ITEM (1 << 7)

#define ACPI_SMALL_ITEM_NAME_IDX 3
#define ACPI_SMALL_ITEM_NAME_MASK 0b1111
#define ACPI_SMALL_ITEM_LENGTH_MASK 0b111

#define ACPI_LARGE_ITEM_NAME_MASK 0b1111111

// Small items
#define ACPI_RESOURCE_IRQ 0x04
#define ACPI_RESOURCE_DMA 0x05
#define ACPI_RESOURCE_START_DEPENDENT 0x06
#define ACPI_RESOURCE_END_DEPENDENT 0x07
#define ACPI_RESOURCE_IO 0x08
#define ACPI_RESOURCE_FIXED_IO 0x09
#define ACPI_RESOURCE_FIXED_DMA 0x0A
#define ACPI_RESOURCE_VENDOR_TYPE0 0x0E
#define ACPI_RESOURCE_END_TAG 0x0F

// Large items
#define ACPI_RESOURCE_MEMORY24 0x01
#define ACPI_RESOURCE_GENERIC_REGISTER 0x02
#define ACPI_RESOURCE_VENDOR_TYPE1 0x04
#define ACPI_RESOURCE_MEMORY32 0x05
#define ACPI_RESOURCE_FIXED_MEMORY32 0x06
#define ACPI_RESOURCE_ADDRESS32 0x07
#define ACPI_RESOURCE_ADDRESS16 0x08
#define ACPI_RESOURCE_EXTENDED_IRQ 0x09
#define ACPI_RESOURCE_ADDRESS64 0x0A
#define ACPI_RESOURCE_ADDRESS64_EXTENDED 0x0B
#define ACPI_RESOURCE_GPIO_CONNECTION 0x0C
#define ACPI_RESOURCE_PIN_FUNCTION 0x0D
#define ACPI_RESOURCE_SERIAL_CONNECTION 0x0E
#define ACPI_RESOURCE_PIN_CONFIGURATION 0x0F
#define ACPI_RESOURCE_PIN_GROUP 0x10
#define ACPI_RESOURCE_PIN_GROUP_FUNCTION 0x11
#define ACPI_RESOURCE_PIN_GROUP_CONFIGURATION 0x12
#define ACPI_RESOURCE_CLOCK_INPUT 0x13

/*
 * Resources as encoded by the raw AML byte stream.
 * For decode API & human usable structures refer to uacpi/resources.h
 */
UACPI_PACKED(struct acpi_small_item {
    uacpi_u8 type_and_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_small_item,  1);

UACPI_PACKED(struct acpi_resource_irq {
    struct acpi_small_item common;
    uacpi_u16 irq_mask;
    uacpi_u8 flags;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_irq, 4);

UACPI_PACKED(struct acpi_resource_dma {
    struct acpi_small_item common;
    uacpi_u8 channel_mask;
    uacpi_u8 flags;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_dma, 3);

UACPI_PACKED(struct acpi_resource_start_dependent {
    struct acpi_small_item common;
    uacpi_u8 flags;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_start_dependent, 2);

UACPI_PACKED(struct acpi_resource_end_dependent {
    struct acpi_small_item common;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_end_dependent, 1);

UACPI_PACKED(struct acpi_resource_io {
    struct acpi_small_item common;
    uacpi_u8 information;
    uacpi_u16 minimum;
    uacpi_u16 maximum;
    uacpi_u8 alignment;
    uacpi_u8 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_io, 8);

UACPI_PACKED(struct acpi_resource_fixed_io {
    struct acpi_small_item common;
    uacpi_u16 address;
    uacpi_u8 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_fixed_io, 4);

UACPI_PACKED(struct acpi_resource_fixed_dma {
    struct acpi_small_item common;
    uacpi_u16 request_line;
    uacpi_u16 channel;
    uacpi_u8 transfer_width;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_fixed_dma, 6);

UACPI_PACKED(struct acpi_resource_vendor_defined_type0 {
    struct acpi_small_item common;
    uacpi_u8 byte_data[];
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_vendor_defined_type0, 1);

UACPI_PACKED(struct acpi_resource_end_tag {
    struct acpi_small_item common;
    uacpi_u8 checksum;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_end_tag, 2);

UACPI_PACKED(struct acpi_large_item {
    uacpi_u8 type;
    uacpi_u16 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_large_item, 3);

UACPI_PACKED(struct acpi_resource_memory24 {
    struct acpi_large_item common;
    uacpi_u8 information;
    uacpi_u16 minimum;
    uacpi_u16 maximum;
    uacpi_u16 alignment;
    uacpi_u16 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_memory24, 12);

UACPI_PACKED(struct acpi_resource_vendor_defined_type1 {
    struct acpi_large_item common;
    uacpi_u8 byte_data[];
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_vendor_defined_type1, 3);

UACPI_PACKED(struct acpi_resource_memory32 {
    struct acpi_large_item common;
    uacpi_u8 information;
    uacpi_u32 minimum;
    uacpi_u32 maximum;
    uacpi_u32 alignment;
    uacpi_u32 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_memory32, 20);

UACPI_PACKED(struct acpi_resource_fixed_memory32 {
    struct acpi_large_item common;
    uacpi_u8 information;
    uacpi_u32 address;
    uacpi_u32 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_fixed_memory32, 12);

UACPI_PACKED(struct acpi_resource_address {
    struct acpi_large_item common;
    uacpi_u8 type;
    uacpi_u8 flags;
    uacpi_u8 type_flags;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_address, 6);

UACPI_PACKED(struct acpi_resource_address64 {
    struct acpi_resource_address common;
    uacpi_u64 granularity;
    uacpi_u64 minimum;
    uacpi_u64 maximum;
    uacpi_u64 translation_offset;
    uacpi_u64 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_address64, 46);

UACPI_PACKED(struct acpi_resource_address32 {
    struct acpi_resource_address common;
    uacpi_u32 granularity;
    uacpi_u32 minimum;
    uacpi_u32 maximum;
    uacpi_u32 translation_offset;
    uacpi_u32 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_address32, 26);

UACPI_PACKED(struct acpi_resource_address16 {
    struct acpi_resource_address common;
    uacpi_u16 granularity;
    uacpi_u16 minimum;
    uacpi_u16 maximum;
    uacpi_u16 translation_offset;
    uacpi_u16 length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_address16, 16);

UACPI_PACKED(struct acpi_resource_address64_extended {
    struct acpi_resource_address common;
    uacpi_u8 revision_id;
    uacpi_u8 reserved;
    uacpi_u64 granularity;
    uacpi_u64 minimum;
    uacpi_u64 maximum;
    uacpi_u64 translation_offset;
    uacpi_u64 length;
    uacpi_u64 attributes;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_address64_extended, 56);

UACPI_PACKED(struct acpi_resource_extended_irq {
    struct acpi_large_item common;
    uacpi_u8 flags;
    uacpi_u8 num_irqs;
    uacpi_u32 irqs[];
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_extended_irq, 5);

UACPI_PACKED(struct acpi_resource_generic_register {
    struct acpi_large_item common;
    uacpi_u8 address_space_id;
    uacpi_u8 bit_width;
    uacpi_u8 bit_offset;
    uacpi_u8 access_size;
    uacpi_u64 address;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_generic_register, 15);

UACPI_PACKED(struct acpi_resource_gpio_connection {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u8 type;
    uacpi_u16 general_flags;
    uacpi_u16 connection_flags;
    uacpi_u8 pull_configuration;
    uacpi_u16 drive_strength;
    uacpi_u16 debounce_timeout;
    uacpi_u16 pin_table_offset;
    uacpi_u8 source_index;
    uacpi_u16 source_offset;
    uacpi_u16 vendor_data_offset;
    uacpi_u16 vendor_data_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_gpio_connection, 23);

#define ACPI_SERIAL_TYPE_I2C 1
#define ACPI_SERIAL_TYPE_SPI 2
#define ACPI_SERIAL_TYPE_UART 3
#define ACPI_SERIAL_TYPE_CSI2 4
#define ACPI_SERIAL_TYPE_MAX ACPI_SERIAL_TYPE_CSI2

UACPI_PACKED(struct acpi_resource_serial {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u8 source_index;
    uacpi_u8 type;
    uacpi_u8 flags;
    uacpi_u16 type_specific_flags;
    uacpi_u8 type_specific_revision_id;
    uacpi_u16 type_data_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_serial, 12);

UACPI_PACKED(struct acpi_resource_serial_i2c {
    struct acpi_resource_serial common;
    uacpi_u32 connection_speed;
    uacpi_u16 slave_address;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_serial_i2c, 18);

UACPI_PACKED(struct acpi_resource_serial_spi {
    struct acpi_resource_serial common;
    uacpi_u32 connection_speed;
    uacpi_u8 data_bit_length;
    uacpi_u8 phase;
    uacpi_u8 polarity;
    uacpi_u16 device_selection;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_serial_spi, 21);

UACPI_PACKED(struct acpi_resource_serial_uart {
    struct acpi_resource_serial common;
    uacpi_u32 baud_rate;
    uacpi_u16 rx_fifo;
    uacpi_u16 tx_fifo;
    uacpi_u8 parity;
    uacpi_u8 lines_enabled;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_serial_uart, 22);

UACPI_PACKED(struct acpi_resource_serial_csi2 {
    struct acpi_resource_serial common;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_serial_csi2, 12);

UACPI_PACKED(struct acpi_resource_pin_function {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u16 flags;
    uacpi_u8 pull_configuration;
    uacpi_u16 function_number;
    uacpi_u16 pin_table_offset;
    uacpi_u8 source_index;
    uacpi_u16 source_offset;
    uacpi_u16 vendor_data_offset;
    uacpi_u16 vendor_data_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_pin_function, 18);

UACPI_PACKED(struct acpi_resource_pin_configuration {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u16 flags;
    uacpi_u8 type;
    uacpi_u32 value;
    uacpi_u16 pin_table_offset;
    uacpi_u8 source_index;
    uacpi_u16 source_offset;
    uacpi_u16 vendor_data_offset;
    uacpi_u16 vendor_data_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_pin_configuration, 20);

UACPI_PACKED(struct acpi_resource_pin_group {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u16 flags;
    uacpi_u16 pin_table_offset;
    uacpi_u16 source_lable_offset;
    uacpi_u16 vendor_data_offset;
    uacpi_u16 vendor_data_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_pin_group, 14);

UACPI_PACKED(struct acpi_resource_pin_group_function {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u16 flags;
    uacpi_u16 function;
    uacpi_u8 source_index;
    uacpi_u16 source_offset;
    uacpi_u16 source_lable_offset;
    uacpi_u16 vendor_data_offset;
    uacpi_u16 vendor_data_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_pin_group_function, 17);

UACPI_PACKED(struct acpi_resource_pin_group_configuration {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u16 flags;
    uacpi_u8 type;
    uacpi_u32 value;
    uacpi_u8 source_index;
    uacpi_u16 source_offset;
    uacpi_u16 source_lable_offset;
    uacpi_u16 vendor_data_offset;
    uacpi_u16 vendor_data_length;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_pin_group_configuration, 20);

UACPI_PACKED(struct acpi_resource_clock_input {
    struct acpi_large_item common;
    uacpi_u8 revision_id;
    uacpi_u16 flags;
    uacpi_u16 divisor;
    uacpi_u32 numerator;
    uacpi_u8 source_index;
})
UACPI_EXPECT_SIZEOF(struct acpi_resource_clock_input, 13);
