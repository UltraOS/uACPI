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
    uacpi_pci_routing_table_entry entries[];
} uacpi_pci_routing_table;
void uacpi_free_pci_routing_table(uacpi_pci_routing_table*);

uacpi_status uacpi_get_pci_routing_table(
    uacpi_namespace_node *parent, uacpi_pci_routing_table **out_table
);

typedef struct uacpi_id_string {
    // size of the string including the null byte
    uacpi_u32 size;
    uacpi_char *value;
} uacpi_id_string;
void uacpi_free_id_string(uacpi_id_string *id);

/*
 * Evaluate a device's _HID method and get its value.
 * The returned struture must be freed using uacpi_free_id_string.
 */
uacpi_status uacpi_eval_hid(uacpi_namespace_node*, uacpi_id_string **out_id);

typedef struct uacpi_pnp_id_list {
    // number of 'ids' in the list
    uacpi_u32 num_ids;

    // size of the 'ids' list including the string lengths
    uacpi_u32 size;

    // list of PNP ids
    uacpi_id_string ids[];
} uacpi_pnp_id_list;
void uacpi_free_pnp_id_list(uacpi_pnp_id_list *list);

/*
 * Evaluate a device's _CID method and get its value.
 * The returned strucutre msut be freed using uacpi_free_pnp_id_list.
 */
uacpi_status uacpi_eval_cid(uacpi_namespace_node*, uacpi_pnp_id_list **out_list);

/*
 * Evaluate a device's _STA method and get its value.
 * If this method is not found, the value of 'flags' is set to all ones.
 */
uacpi_status uacpi_eval_sta(uacpi_namespace_node*, uacpi_u32 *flags);

#ifdef __cplusplus
}
#endif
