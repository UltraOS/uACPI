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
    UACPI_OBJECT_NULL = 0,
    UACPI_OBJECT_INTEGER = 1,
    UACPI_OBJECT_STRING = 2,
    UACPI_OBJECT_BUFFER = 3,
    UACPI_OBJECT_PACKAGE = 4,
    UACPI_OBJECT_REFERENCE = 5,
    UACPI_OBJECT_METHOD = 6,
    UACPI_OBJECT_SPECIAL = 7,
} uacpi_object_type;

#define UACPI_OBJECT_COMMON_HDR \
    uacpi_u8 type;              \
    uacpi_u8 flags;             \
    uacpi_u32 refcount;         \

typedef union uacpi_object uacpi_object;

typedef struct uacpi_object_integer {
    UACPI_OBJECT_COMMON_HDR
    uacpi_u64 value;
} uacpi_object_integer;

typedef struct uacpi_object_string {
    UACPI_OBJECT_COMMON_HDR
    uacpi_char *text;
    uacpi_size length;
} uacpi_object_string;

typedef struct uacpi_object_buffer {
    UACPI_OBJECT_COMMON_HDR
    void *data;
    uacpi_size size;
} uacpi_object_buffer;

typedef struct uacpi_object_package {
    UACPI_OBJECT_COMMON_HDR
    uacpi_object *objects;
    uacpi_size count;
} uacpi_object_package;

typedef struct uacpi_object_reference {
    UACPI_OBJECT_COMMON_HDR
    uacpi_object *object;
} uacpi_object_reference;

typedef struct uacpi_object_control_method {
    UACPI_OBJECT_COMMON_HDR
    uacpi_control_method *method;
} uacpi_object_control_method;

enum uacpi_special_type {
    UACPI_SPECIAL_TYPE_DEBUG_OBJECT = 1,
    UACPI_SPECIAL_TYPE_TIMER_OBJECT = 2,
};

typedef struct uacpi_object_special {
    UACPI_OBJECT_COMMON_HDR
    uacpi_u8 special_type;
} uacpi_object_special;

typedef struct uacpi_object_common {
    UACPI_OBJECT_COMMON_HDR
} uacpi_object_common;

typedef union uacpi_object {
    uacpi_u8 type;
    uacpi_object_common common;
    uacpi_object_integer as_integer;
    uacpi_object_string as_string;
    uacpi_object_buffer as_buffer;
    uacpi_object_package as_package;
    uacpi_object_reference as_reference;
    uacpi_object_control_method as_method;
    uacpi_object_special as_special;
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
