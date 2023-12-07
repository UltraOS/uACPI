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
    UACPI_OBJECT_METHOD = 8,
    UACPI_OBJECT_BUFFER_FIELD = 14,
    UACPI_OBJECT_DEBUG = 16,

    UACPI_OBJECT_REFERENCE = 20,
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
    struct uacpi_shareable shareable;
    uacpi_buffer *backing;
    uacpi_size bit_index;
    uacpi_u32 bit_length;
    uacpi_bool force_buffer;
} uacpi_buffer_field;

typedef struct uacpi_object {
    struct uacpi_shareable shareable;
    uacpi_u8 type;
    uacpi_u8 flags;

    union {
        uacpi_u64 integer;
        uacpi_package *package;
        uacpi_buffer_field *buffer_field;
        uacpi_object *inner_object;
        uacpi_control_method *method;
        uacpi_buffer *buffer;
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
