#pragma once
#include <uacpi/platform/types.h>
#include <uacpi/platform/compiler.h>
#include <uacpi/platform/arch_helpers.h>
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

typedef struct uacpi_pci_address {
    uacpi_u16 segment;
    uacpi_u8 bus;
    uacpi_u8 device;
    uacpi_u8 function;
} uacpi_pci_address;

typedef void *uacpi_handle;
typedef struct uacpi_namespace_node uacpi_namespace_node;

typedef enum uacpi_object_type {
    UACPI_OBJECT_UNINITIALIZED = 0,
    UACPI_OBJECT_INTEGER = 1,
    UACPI_OBJECT_STRING = 2,
    UACPI_OBJECT_BUFFER = 3,
    UACPI_OBJECT_PACKAGE = 4,
    UACPI_OBJECT_FIELD_UNIT = 5,
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
#define UACPI_OBJECT_FIELD_UNIT_BIT (1 << UACPI_OBJECT_FIELD_UNIT)
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
        uacpi_u8 *byte_data;
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
    uacpi_thread_id owner;
    uacpi_u16 depth;
    uacpi_u8 sync_level;
} uacpi_mutex;

typedef struct uacpi_event {
    struct uacpi_shareable shareable;
    uacpi_handle handle;
} uacpi_event;

typedef enum uacpi_region_op {
    UACPI_REGION_OP_ATTACH = 1,
    UACPI_REGION_OP_READ = 2,
    UACPI_REGION_OP_WRITE = 3,
    UACPI_REGION_OP_DETACH = 4,
} uacpi_region_op;

typedef struct uacpi_region_attach_data {
    void *handler_context;
    uacpi_namespace_node *region_node;
    void *out_region_context;
} uacpi_region_attach_data;

typedef struct uacpi_region_rw_data {
    void *handler_context;
    void *region_context;
    union {
        uacpi_phys_addr address;
        uacpi_u64 offset;
    };
    uacpi_u64 value;
    uacpi_u8 byte_width;
} uacpi_region_rw_data;

typedef struct uacpi_region_detach_data {
    void *handler_context;
    void *region_context;
    uacpi_namespace_node *region_node;
} uacpi_region_detach_data;

typedef uacpi_status (*uacpi_region_handler)
    (uacpi_region_op op, uacpi_handle op_data);

typedef struct uacpi_address_space_handler {
    struct uacpi_shareable shareable;
    uacpi_region_handler callback;
    uacpi_handle user_context;
    struct uacpi_address_space_handler *next;
    struct uacpi_operation_region *regions;
    uacpi_u16 space;
} uacpi_address_space_handler;

typedef uacpi_status (*uacpi_notify_handler)
    (uacpi_handle context, uacpi_namespace_node *node, uacpi_u64 value);

typedef struct uacpi_device_notify_handler {
    uacpi_notify_handler callback;
    uacpi_handle user_context;
    struct uacpi_device_notify_handler *next;
} uacpi_device_notify_handler;

/*
 * NOTE: These are common object headers.
 * Any changes to these structs must be propagated to all objects.
 * ==============================================================
 * Common for the following objects:
 * - UACPI_OBJECT_OPERATION_REGION
 * - UACPI_OBJECT_PROCESSOR
 * - UACPI_OBJECT_DEVICE
 * - UACPI_OBJECT_THERMAL_ZONE
 */
typedef struct uacpi_address_space_handlers {
    struct uacpi_shareable shareable;
    uacpi_address_space_handler *head;
} uacpi_address_space_handlers;

/*
 * Common for the following objects:
 * - UACPI_OBJECT_PROCESSOR
 * - UACPI_OBJECT_DEVICE
 * - UACPI_OBJECT_THERMAL_ZONE
 */
typedef struct uacpi_handlers {
    struct uacpi_shareable shareable;
    uacpi_address_space_handler *address_space_head;
    uacpi_device_notify_handler *notify_head;
} uacpi_handlers;

typedef enum uacpi_address_space {
    UACPI_ADDRESS_SPACE_SYSTEM_MEMORY = 0,
    UACPI_ADDRESS_SPACE_SYSTEM_IO = 1,
    UACPI_ADDRESS_SPACE_PCI_CONFIG = 2,
    UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER = 3,
    UACPI_ADDRESS_SPACE_SMBUS = 4,
    UACPI_ADDRESS_SPACE_SYSTEM_CMOS = 5,
    UACPI_ADDRESS_SPACE_PCI_BAR_TARGET = 6,
    UACPI_ADDRESS_SPACE_IPMI = 7,
    UACPI_ADDRESS_SPACE_GENERAL_PURPOSE_IO = 8,
    UACPI_ADDRESS_SPACE_GENERIC_SERIAL_BUS = 9,
    UACPI_ADDRESS_SPACE_PCC = 0x0A,
    UACPI_ADDRESS_SPACE_PRM = 0x0B,
    UACPI_ADDRESS_SPACE_FFIXEDHW = 0x7F,

    // Internal type
    UACPI_ADDRESS_SPACE_TABLE_DATA = 0xDA1A,
} uacpi_address_space;
const uacpi_char *uacpi_address_space_to_string(uacpi_address_space space);

// This region has a corresponding _REG method that was succesfully executed
#define UACPI_OP_REGION_STATE_REG_EXECUTED (1 << 0)

// This region was successfully attached to a handler
#define UACPI_OP_REGION_STATE_ATTACHED (1 << 1)

typedef struct uacpi_operation_region {
    struct uacpi_shareable shareable;
    uacpi_address_space_handler *handler;
    uacpi_handle user_context;
    uacpi_u16 space;
    uacpi_u8 state_flags;
    uacpi_u64 offset;
    uacpi_u64 length;

    // Used to link regions sharing the same handler
    struct uacpi_operation_region *next;
} uacpi_operation_region;

typedef struct uacpi_device {
    struct uacpi_shareable shareable;
    uacpi_address_space_handler *address_space_handlers;
    uacpi_device_notify_handler *notify_handlers;
} uacpi_device;

typedef struct uacpi_processor {
    struct uacpi_shareable shareable;
    uacpi_address_space_handler *address_space_handlers;
    uacpi_device_notify_handler *notify_handlers;
    uacpi_u8 id;
    uacpi_u32 block_address;
    uacpi_u8 block_length;
} uacpi_processor;

typedef struct uacpi_thermal_zone {
    struct uacpi_shareable shareable;
    uacpi_address_space_handler *address_space_handlers;
    uacpi_device_notify_handler *notify_handlers;
} uacpi_thermal_zone;

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

typedef enum uacpi_field_unit_kind {
    UACPI_FIELD_UNIT_KIND_NORMAL = 0,
    UACPI_FIELD_UNIT_KIND_INDEX = 1,
    UACPI_FIELD_UNIT_KIND_BANK = 2,
} uacpi_field_unit_kind;

typedef struct uacpi_field_unit {
    struct uacpi_shareable shareable;

    union {
        // UACPI_FIELD_UNIT_KIND_NORMAL
        struct {
            uacpi_namespace_node *region;
        };

        // UACPI_FIELD_UNIT_KIND_INDEX
        struct {
            struct uacpi_field_unit *index;
            struct uacpi_field_unit *data;
        };

        // UACPI_FIELD_UNIT_KIND_BANK
        struct {
            uacpi_namespace_node *bank_region;
            struct uacpi_field_unit *bank_selection;
            uacpi_u64 bank_value;
        };
    };

    uacpi_object *connection;

    uacpi_u32 byte_offset;
    uacpi_u32 bit_length;
    uacpi_u8 bit_offset_within_first_byte;
    uacpi_u8 access_width_bytes;
    uacpi_u8 access_length;

    uacpi_u8 attributes : 4;
    uacpi_u8 update_rule : 2;
    uacpi_u8 kind : 2;
    uacpi_u8 lock_rule : 1;
} uacpi_field_unit;

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
        uacpi_device *device;
        uacpi_processor *processor;
        uacpi_thermal_zone *thermal_zone;
        uacpi_address_space_handlers *address_space_handlers;
        uacpi_handlers *handlers;
        uacpi_power_resource power_resource;
        uacpi_field_unit *field_unit;
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

typedef enum uacpi_firmware_request_type {
    UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT,
    UACPI_FIRMWARE_REQUEST_TYPE_FATAL,
} uacpi_firmware_request_type;

typedef struct uacpi_firmware_request {
    uacpi_u8 type;

    union {
        // UACPI_FIRMWARE_REQUEST_BREAKPOINT
        struct {
            // The context of the method currently being executed
            uacpi_handle ctx;
        } breakpoint;

        // UACPI_FIRMWARE_REQUEST_FATAL
        struct {
            uacpi_u8 type;
            uacpi_u32 code;
            uacpi_u64 arg;
        } fatal;
    };
} uacpi_firmware_request;

#define UACPI_INTERRUPT_NOT_HANDLED 0
#define UACPI_INTERRUPT_HANDLED 1
typedef uacpi_u32 uacpi_interrupt_ret;

typedef uacpi_interrupt_ret (*uacpi_interrupt_handler)(uacpi_handle);

uacpi_object *uacpi_create_object(uacpi_object_type type);

void uacpi_object_ref(uacpi_object *obj);
void uacpi_object_unref(uacpi_object *obj);

#ifdef __cplusplus
}
#endif
