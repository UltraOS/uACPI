#pragma once
#include <uacpi/platform/types.h>
#include <uacpi/platform/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

#if UACPI_POINTER_SIZE == 4 && defined(UACPI_PHYS_ADDR_IS_32BITS)
typedef uacpi_u32 uacpi_phys_addr;
typedef uacpi_u32 uacpi_io_addr;
#else
typedef uacpi_u64 uacpi_phys_addr;
typedef uacpi_u64 uacpi_io_addr;
#endif

typedef void *uacpi_handle;
typedef struct uacpi_control_method uacpi_control_method;

typedef enum uacpi_object_type {
    UACPI_OBJECT_UNINITIALIZED = 0,
    UACPI_OBJECT_INTEGER = 1,
    UACPI_OBJECT_STRING = 2,
    UACPI_OBJECT_BUFFER = 3,
    UACPI_OBJECT_PACKAGE = 4,
    UACPI_OBJECT_DEVICE = 6,
    UACPI_OBJECT_METHOD = 8,
    UACPI_OBJECT_OPERATION_REGION = 10,
    UACPI_OBJECT_POWER_RESOURCE = 11,
    UACPI_OBJECT_PROCESSOR = 12,
    UACPI_OBJECT_THERMAL_ZONE = 13,
    UACPI_OBJECT_BUFFER_FIELD = 14,
    UACPI_OBJECT_DEBUG = 16,

    UACPI_OBJECT_REFERENCE = 20,
    UACPI_OBJECT_BUFFER_INDEX = 21,
} uacpi_object_type;

const uacpi_char *uacpi_object_type_to_string(uacpi_object_type);
typedef struct uacpi_object uacpi_object;

struct uacpi_shareable {
    uacpi_u32 reference_count;
};

typedef struct uacpi_buffer {
    struct uacpi_shareable shareable;
    union {
        void *data;
        uacpi_char *text;
    };
    uacpi_size size;
} uacpi_buffer;

typedef struct uacpi_package {
    struct uacpi_shareable shareable;
    uacpi_object **objects;
    uacpi_size count;
} uacpi_package;

typedef struct uacpi_buffer_field {
    uacpi_buffer *backing;
    uacpi_size bit_index;
    uacpi_u32 bit_length;
    uacpi_bool force_buffer;
} uacpi_buffer_field;

typedef struct uacpi_buffer_index {
    uacpi_size idx;
    uacpi_buffer *buffer;
} uacpi_buffer_index;

enum uacpi_operation_region_space {
    UACPI_OP_REGION_SPACE_SYSTEM_MEMORY = 0,
    UACPI_OP_REGION_SPACE_SYSTEM_IO = 1,
    UACPI_OP_REGION_SPACE_PCI_CONFIG = 2,
    UACPI_OP_REGION_SPACE_EMBEDDED_CONTROLLER = 3,
    UACPI_OP_REGION_SPACE_SMBUS = 4,
    UACPI_OP_REGION_SPACE_SYSTEM_CMOS = 5,
    UACPI_OP_REGION_SPACE_PCI_BAR_TARGET = 6,
    UACPI_OP_REGION_SPACE_IPMI = 7,
    UACPI_OP_REGION_SPACE_GENERAL_PURPOSE_IO = 8,
    UACPI_OP_REGION_SPACE_GENERIC_SERIAL_BUS = 9,
    UACPI_OP_REGION_SPACE_PCC = 0x0A,
};

typedef struct uacpi_operation_region {
    uacpi_u8 space;
    uacpi_u64 offset;
    uacpi_u64 length;
} uacpi_operation_region;

typedef struct uacpi_processor {
    uacpi_u8 id;
    uacpi_u32 block_address;
    uacpi_u8 block_length;
} uacpi_processor;

typedef struct uacpi_power_resource {
    uacpi_u8 system_level;
    uacpi_u16 resource_order;
} uacpi_power_resource;

typedef struct uacpi_object {
    struct uacpi_shareable shareable;
    uacpi_u8 type;
    uacpi_u8 flags;

    union {
        uacpi_u64 integer;
        uacpi_package *package;
        uacpi_buffer_field buffer_field;
        uacpi_object *inner_object;
        uacpi_control_method *method;
        uacpi_buffer *buffer;
        uacpi_buffer_index buffer_index;
        uacpi_operation_region op_region;
        uacpi_processor processor;
        uacpi_power_resource power_resource;
    };
} uacpi_object;

typedef struct uacpi_args {
    uacpi_object **objects;
    uacpi_size count;
} uacpi_args;

typedef union uacpi_object_name {
    uacpi_char text[4];
    uacpi_u32 id;
} uacpi_object_name;

uacpi_object *uacpi_create_object(uacpi_object_type type);

void uacpi_object_ref(uacpi_object *obj);
void uacpi_object_unref(uacpi_object *obj);

#ifdef __cplusplus
}
#endif
