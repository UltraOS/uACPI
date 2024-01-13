#pragma once

#include <uacpi/internal/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uacpi_status {
    UACPI_STATUS_OK = 0,
    UACPI_STATUS_MAPPING_FAILED = 1,
    UACPI_STATUS_OUT_OF_MEMORY = 2,
    UACPI_STATUS_BAD_CHECKSUM = 3,
    UACPI_STATUS_INVALID_SIGNATURE = 4,
    UACPI_STATUS_INVALID_TABLE_LENGTH = 5,
    UACPI_STATUS_NOT_FOUND = 6,
    UACPI_STATUS_INVALID_ARGUMENT = 7,
    UACPI_STATUS_BAD_BYTECODE = 8,
    UACPI_STATUS_UNIMPLEMENTED = 9,
    UACPI_STATUS_ALREADY_EXISTS = 10,
    UACPI_STATUS_INTERNAL_ERROR = 11,
    UACPI_STATUS_TYPE_MISMATCH = 12,

    // All errors that have bytecode-related origin should go here
    UACPI_STATUS_AML_UNDEFINED_REFERENCE = 0xEFFF0000,
    UACPI_STATUS_AML_INVALID_NAMESTRING = 0xEFFF0001,
    UACPI_STATUS_AML_OBJECT_ALREADY_EXISTS = 0xEFFF0002,
    UACPI_STATUS_AML_INVALID_OPCODE = 0xEFFF0003,
    UACPI_STATUS_AML_INCOMPATIBLE_OBJECT_TYPE = 0xEFFF0004,
    UACPI_STATUS_AML_BAD_ENCODING = 0xEFFF0005,
    UACPI_STATUS_AML_OUT_OF_BOUNDS_INDEX = 0xEFFF0006,
    UACPI_STATUS_AML_SYNC_LEVEL_TOO_HIGH = 0xEFFF0007,
} uacpi_status;

const char *uacpi_status_to_string(uacpi_status);

#define uacpi_unlikely_error(expr) uacpi_unlikely((expr) != UACPI_STATUS_OK)
#define uacpi_likely_error(expr)   uacpi_likely((expr) != UACPI_STATUS_OK)

#define uacpi_unlikely_success(expr) uacpi_unlikely((expr) == UACPI_STATUS_OK)
#define uacpi_likely_success(expr)   uacpi_likely((expr) == UACPI_STATUS_OK)

#ifdef __cplusplus
}
#endif