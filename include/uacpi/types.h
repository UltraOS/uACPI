#pragma once
#include <uacpi/platform/types.h>
#include <uacpi/platform/compiler.h>
#include <uacpi/status.h>

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

typedef enum uacpi_object_type {
    UACPI_OBJECT_UNINITIALIZED = 0,
    UACPI_OBJECT_INTEGER = 1,
    UACPI_OBJECT_STRING = 2,
    UACPI_OBJECT_BUFFER = 3,
    UACPI_OBJECT_PACKAGE = 4,
    UACPI_OBJECT_UNIT_FIELD = 5,
    UACPI_OBJECT_DEVICE = 6,
    UACPI_OBJECT_EVENT = 7,
    UACPI_OBJECT_METHOD = 8,
    UACPI_OBJECT_MUTEX = 9,
    UACPI_OBJECT_OPERATION_REGION = 10,
    UACPI_OBJECT_POWER_RESOURCE = 11,
    UACPI_OBJECT_PROCESSOR = 12,
    UACPI_OBJECT_THERMAL_ZONE = 13,
    UACPI_OBJECT_BUFFER_FIELD = 14,
    UACPI_OBJECT_DEBUG = 16,

    UACPI_OBJECT_REFERENCE = 20,
    UACPI_OBJECT_BUFFER_INDEX = 21,
    UACPI_OBJECT_MAX_TYPE_VALUE = UACPI_OBJECT_BUFFER_INDEX
} uacpi_object_type;

// Type bits for API requiring a bit mask, e.g. uacpi_eval_typed
#define UACPI_OBJECT_INTEGER_BIT (1 << UACPI_OBJECT_INTEGER)
#define UACPI_OBJECT_STRING_BIT (1 << UACPI_OBJECT_STRING)
#define UACPI_OBJECT_BUFFER_BIT (1 << UACPI_OBJECT_BUFFER)
#define UACPI_OBJECT_PACKAGE_BIT (1 << UACPI_OBJECT_PACKAGE)
#define UACPI_OBJECT_UNIT_FIELD_BIT (1 << UACPI_OBJECT_UNIT_FIELD)
#define UACPI_OBJECT_DEVICE_BIT (1 << UACPI_OBJECT_DEVICE)
#define UACPI_OBJECT_EVENT_BIT (1 << UACPI_OBJECT_EVENT)
#define UACPI_OBJECT_METHOD_BIT (1 << UACPI_OBJECT_METHOD)
#define UACPI_OBJECT_MUTEX_BIT (1 << UACPI_OBJECT_MUTEX)
#define UACPI_OBJECT_OPERATION_REGION_BIT (1 << UACPI_OBJECT_OPERATION_REGION)
#define UACPI_OBJECT_POWER_RESOURCE_BIT (1 << UACPI_OBJECT_POWER_RESOURCE)
#define UACPI_OBJECT_PROCESSOR_BIT (1 << UACPI_OBJECT_PROCESSOR)
#define UACPI_OBJECT_THERMAL_ZONE_BIT (1 << UACPI_OBJECT_THERMAL_ZONE)
#define UACPI_OBJECT_BUFFER_FIELD_BIT (1 << UACPI_OBJECT_BUFFER_FIELD)
#define UACPI_OBJECT_DEBUG_BIT (1 << UACPI_OBJECT_DEBUG)
#define UACPI_OBJECT_REFERENCE_BIT (1 << UACPI_OBJECT_REFERENCE)
#define UACPI_OBJECT_BUFFER_INDEX_BIT (1 << UACPI_OBJECT_BUFFER_INDEX)

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

typedef struct uacpi_mutex {
    struct uacpi_shareable shareable;
    uacpi_handle handle;
    uacpi_handle owner;
    uacpi_u64 depth;
    uacpi_u8 sync_level;
} uacpi_mutex;

typedef struct uacpi_event {
    struct uacpi_shareable shareable;
    uacpi_handle handle;
} uacpi_event;

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

    // Internal type
    UACPI_OP_REGION_SPACE_TABLE_DATA = 0xFE,
};

typedef struct uacpi_operation_region {
    struct uacpi_shareable shareable;
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

typedef uacpi_status (*uacpi_native_call_handler)(
    uacpi_handle ctx, uacpi_object *retval
);

typedef struct uacpi_control_method {
    struct uacpi_shareable shareable;
    union {
        uacpi_u8 *code;
        uacpi_native_call_handler handler;
    };
    uacpi_mutex *mutex;
    uacpi_u32 size;
    uacpi_u8 sync_level : 4;
    uacpi_u8 args : 3;
    uacpi_u8 is_serialized : 1;
    uacpi_u8 named_objects_persist: 1;
    uacpi_u8 native_call : 1;
} uacpi_control_method;

typedef enum uacpi_access_type {
    UACPI_ACCESS_TYPE_ANY = 0,
    UACPI_ACCESS_TYPE_BYTE = 1,
    UACPI_ACCESS_TYPE_WORD = 2,
    UACPI_ACCESS_TYPE_DWORD = 3,
    UACPI_ACCESS_TYPE_QWORD = 4,
    UACPI_ACCESS_TYPE_BUFFER = 5,
} uacpi_access_type;

typedef enum uacpi_access_attributes {
    UACPI_ACCESS_ATTRIBUTE_QUICK = 0x02,
    UACPI_ACCESS_ATTRIBUTE_SEND_RECEIVE = 0x04,
    UACPI_ACCESS_ATTRIBUTE_BYTE = 0x06,
    UACPI_ACCESS_ATTRIBUTE_WORD = 0x08,
    UACPI_ACCESS_ATTRIBUTE_BLOCK = 0x0A,
    UACPI_ACCESS_ATTRIBUTE_BYTES = 0x0B,
    UACPI_ACCESS_ATTRIBUTE_PROCESS_CALL = 0x0C,
    UACPI_ACCESS_ATTRIBUTE_BLOCK_PROCESS_CALL = 0x0D,
    UACPI_ACCESS_ATTRIBUTE_RAW_BYTES = 0x0E,
    UACPI_ACCESS_ATTRIBUTE_RAW_PROCESS_BYTES = 0x0F,
} uacpi_access_attributes;

typedef enum uacpi_lock_rule {
    UACPI_LOCK_RULE_NO_LOCK = 0,
    UACPI_LOCK_RULE_LOCK = 1,
} uacpi_lock_rule;

typedef enum uacpi_update_rule {
    UACPI_UPDATE_RULE_PRESERVE = 0,
    UACPI_UPDATE_RULE_WRITE_AS_ONES = 1,
    UACPI_UPDATE_RULE_WRITE_AS_ZEROES = 2,
} uacpi_update_rule;

typedef enum uacpi_unit_field_kind {
    UACPI_UNIT_FIELD_KIND_NORMAL = 0,
    UACPI_UNIT_FIELD_KIND_INDEX = 1,
    UACPI_UNIT_FIELD_KIND_BANK = 2,
} uacpi_unit_field_kind;

typedef struct uacpi_unit_field {
    struct uacpi_shareable shareable;

    union {
        // UACPI_UNIT_FIELD_KIND_NORMAL
        struct {
            uacpi_operation_region *region;
        };

        // UACPI_UNIT_FIELD_KIND_INDEX
        struct {
            struct uacpi_unit_field *index;
            struct uacpi_unit_field *data;
        };

        // UACPI_UNIT_FIELD_KIND_BANK
        struct {
            uacpi_operation_region *bank_region;
            struct uacpi_unit_field *bank_selection;
            uacpi_u64 bank_value;
        };
    };

    uacpi_object *connection;

    uacpi_u32 bit_offset;
    uacpi_u32 bit_length;

    uacpi_u8 attributes : 4;
    uacpi_u8 access_type : 3;
    uacpi_u8 lock_rule : 1;
    uacpi_u8 update_rule : 2;
    uacpi_u8 kind : 2;
    uacpi_u8 access_length;
} uacpi_unit_field;

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
        uacpi_mutex *mutex;
        uacpi_event *event;
        uacpi_buffer_index buffer_index;
        uacpi_operation_region *op_region;
        uacpi_processor processor;
        uacpi_power_resource power_resource;
        uacpi_unit_field *unit_field;
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
