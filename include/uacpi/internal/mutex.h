#pragma once

#include <uacpi/types.h>

uacpi_bool uacpi_this_thread_owns_aml_mutex(uacpi_mutex*);

uacpi_bool uacpi_acquire_aml_mutex(uacpi_mutex*, uacpi_u16 timeout);
void uacpi_release_aml_mutex(uacpi_mutex*);
