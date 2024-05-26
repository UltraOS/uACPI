#pragma once

#include <uacpi/platform/types.h>

#define UACPI_DEFAULT_LOOP_TIMEOUT_SECONDS 30
#define UACPI_DEFAULT_MAX_CALL_STACK_DEPTH 256

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Set the maximum number of seconds a While loop is allowed to run for before
 * getting timed out.
 *
 * 0 is treated a special value that resets the setting to the default value.
 */
void uacpi_context_set_loop_timeout(uacpi_u32 seconds);

/*
 * Set the maximum call stack depth AML can reach before getting aborted.
 *
 * 0 is treated as a special value that resets the setting to the default value.
 */
void uacpi_context_set_max_call_stack_depth(uacpi_u32 depth);

uacpi_u32 uacpi_context_get_loop_timeout(void);

#ifdef __cplusplus
}
#endif
