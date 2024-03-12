#pragma once

#ifdef UACPI_OVERRIDE_ARCH_HELPERS
#include "uacpi_arch_helpers.h"
#else

#ifndef UACPI_ARCH_FLUSH_CPU_CACHE
#define UACPI_ARCH_FLUSH_CPU_CACHE() do {} while (0)
#endif

typedef unsigned long uacpi_cpu_flags;

#endif
