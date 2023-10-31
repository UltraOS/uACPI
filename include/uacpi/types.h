#pragma once
#include <uacpi/platform/types.h>
#include <uacpi/platform/compiler.h>

#if UACPI_POINTER_SIZE == 4 && defined(UACPI_PHYS_ADDR_IS_32BITS)
typedef uacpi_u32 uacpi_phys_addr;
typedef uacpi_u32 uacpi_io_addr;
#else
typedef uacpi_u64 uacpi_phys_addr;
typedef uacpi_u64 uacpi_io_addr;
#endif

typedef void *uacpi_handle;

typedef struct uacpi_buffer {
    void *data;
    uacpi_size size;
} uacpi_buffer;

/*
 * uACPI will automatically attempt to allocate the storage of neccesary
 * size with uacpi_kernel_alloc when it's needed. The buffer is considered
 * fixed size & caller-owned otherwise.
 */
#define UACPI_DYNAMIC_BUFFER_SIZE ((uacpi_size)-1)

static inline void uacpi_make_buffer_dynamic(uacpi_buffer *buf) {
    buf->size = UACPI_DYNAMIC_BUFFER_SIZE;
}

static inline uacpi_bool uacpi_is_dynamic_buffer(const uacpi_buffer *buf) {
    return buf->size == UACPI_DYNAMIC_BUFFER_SIZE;
}

typedef enum uacpi_object_type {
    UACPI_OBJECT_NULL      = 0,
    UACPI_OBJECT_INTEGER   = 1,
    UACPI_OBJECT_STRING    = 2,
    UACPI_OBJECT_BUFFER    = 3,
    UACPI_OBJECT_PACKAGE   = 4,
    UACPI_OBJECT_REFERENCE = 5,
} uacpi_object_type;

typedef union uacpi_object uacpi_object;

typedef struct uacpi_object_integer {
    uacpi_object_type type;
    uacpi_u64 value;
} uacpi_object_integer;

typedef struct uacpi_object_string {
    uacpi_object_type type;
    uacpi_char *text;
    uacpi_size length;
} uacpi_object_string;

typedef struct uacpi_object_buffer {
    uacpi_object_type type;
    void *data;
    uacpi_size size;
} uacpi_object_buffer;

typedef struct uacpi_object_package {
    uacpi_object_type type;
    uacpi_object *objects;
    uacpi_size count;
} uacpi_object_package;

typedef struct uacpi_object_reference {
    uacpi_object_type type;
    uacpi_object_type referenced_type;
    uacpi_handle handle;
} uacpi_object_reference;

typedef union uacpi_object {
    uacpi_object_type type;

    uacpi_object_integer as_integer;
    uacpi_object_string as_string;
    uacpi_object_buffer as_buffer;
    uacpi_object_package as_package;
} uacpi_object;

typedef struct uacpi_args {
    uacpi_object *objects;
    uacpi_size count;
} uacpi_args;

typedef struct uacpi_retval {
    union {
        // IN filled by the caller
        uacpi_buffer receiving_buffer;

        // OUT filled by the callee using receiving_buffer or inline capacity
        uacpi_object object;
    };
} uacpi_retval;

typedef union uacpi_object_name {
    uacpi_char text[4];
    uacpi_u32 id;
} uacpi_object_name;
