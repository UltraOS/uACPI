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

#ifdef __cplusplus
}
#endif
