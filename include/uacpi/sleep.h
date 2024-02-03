#pragma once

#include <uacpi/types.h>
#include <uacpi/status.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uacpi_sleep_state {
    UACPI_SLEEP_STATE_S0 = 0,
    UACPI_SLEEP_STATE_S1,
    UACPI_SLEEP_STATE_S2,
    UACPI_SLEEP_STATE_S3,
    UACPI_SLEEP_STATE_S4,
    UACPI_SLEEP_STATE_S5,
    UACPI_SLEEP_STATE_MAX = UACPI_SLEEP_STATE_S5,
} uacpi_sleep_state;

/*
 * Prepare for a given sleep state.
 * Must be caled with interrupts ENABLED.
 */
uacpi_status uacpi_prepare_for_sleep_state(enum uacpi_sleep_state);

#ifdef __cplusplus
}
#endif
