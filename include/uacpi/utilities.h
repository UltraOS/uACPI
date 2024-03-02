#pragma once

#include <uacpi/status.h>
#include <uacpi/types.h>
#include <uacpi/namespace.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Checks whether the device at 'node' matches any of the PNP ids provided in
 * 'list'. This is done by first attempting to match the value retruned
 * from _HID and then the value(s) from _CID.
 *
 * Note that the presence of the device (_STA) is not verified here.
 */
uacpi_bool uacpi_device_matches_pnp_id(
    uacpi_namespace_node *node,
    const uacpi_char **list,
    uacpi_size num_entries
);

/*
 * Find all the devices in the namespace matching the specified 'hid', only
 * devices reported as present via _STA are checked. Any matching devices
 * are then passed to the 'cb'.
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

#ifdef __cplusplus
}
#endif
