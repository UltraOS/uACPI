#pragma once

#include <uacpi/status.h>
#include <uacpi/types.h>
#include <uacpi/namespace.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Checks whether the device at 'node' matches any of the PNP ids provided in
 * 'list' (terminated by a UACPI_NULL). This is done by first attempting to
 * match the value retruned from _HID and then the value(s) from _CID.
 *
 * Note that the presence of the device (_STA) is not verified here.
 */
uacpi_bool uacpi_device_matches_pnp_id(
    uacpi_namespace_node *node,
    const uacpi_char **list
);

/*
 * Find all the devices in the namespace starting at 'parent' matching the
 * specified 'hids' (terminated by a UACPI_NULL). Only devices reported as
 * present via _STA are checked. Any matching devices are then passed to
 * the 'cb'.
 */
uacpi_status uacpi_find_devices_at(
    uacpi_namespace_node *parent,
    const uacpi_char **hids,
    uacpi_iteration_callback cb,
    void *user
);

/*
 * Same as uacpi_find_devices_at, except this starts at the root and only
 * matches one hid.
 */
uacpi_status uacpi_find_devices(
    const uacpi_char *hid,
    uacpi_iteration_callback cb,
    void *user
);

typedef enum uacpi_interrupt_model {
    UACPI_INTERRUPT_MODEL_PIC = 0,
    UACPI_INTERRUPT_MODEL_IOAPIC = 1,
    UACPI_INTERRUPT_MODEL_IOSAPIC = 2,
} uacpi_interrupt_model;

uacpi_status uacpi_set_interrupt_model(uacpi_interrupt_model);

typedef struct uacpi_pci_routing_table_entry {
    uacpi_u32 address;
    uacpi_u32 index;
    uacpi_namespace_node *source;
    uacpi_u8 pin;
} uacpi_pci_routing_table_entry;

typedef struct uacpi_pci_routing_table {
    uacpi_size num_entries;
    uacpi_pci_routing_table_entry *entries;
} uacpi_pci_routing_table;

uacpi_status uacpi_get_pci_routing_table(
    uacpi_namespace_node *parent, uacpi_pci_routing_table *out_table
);

#ifdef __cplusplus
}
#endif
