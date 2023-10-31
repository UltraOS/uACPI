#pragma once

#include <uacpi/types.h>

#define UACPI_EXTENDED_OPCODE_INFO

typedef uacpi_u16 uacpi_aml_op;

enum uacpi_opcode_type {
    UACPI_OPCODE_TYPE_ARG = 0,
    UACPI_OPCODE_TYPE_EXEC = 1,
    UACPI_OPCODE_TYPE_FLOW = 2,
    UACPI_OPCODE_TYPE_CREATE = 3,
    UACPI_OPCODE_TYPE_METHOD_CALL = 4,
};

enum uacpi_arg_type {
    UACPI_ARG_TYPE_NUMBER = 0,
    UACPI_ARG_TYPE_STRING = 1,
    UACPI_ARG_TYPE_SPECIAL = 2,
};

enum uacpi_arg_sub_type {
    UACPI_ARG_SUB_TYPE_CONSTANT = 0,
    UACPI_ARG_SUB_TYPE_LOCAL = 1,
    UACPI_ARG_SUB_TYPE_ARG = 2,
};

struct uacpi_opcode_arg {
    enum uacpi_opcode_type type : 4;
    enum uacpi_arg_type arg_type : 3;
    enum uacpi_arg_sub_type sub_type : 3;
};

struct uacpi_opcode_exec {
    enum uacpi_opcode_type type : 4;
    uacpi_u8 operand_count : 3;
    bool has_target : 1;
    bool has_ret : 1;
};

struct uacpi_opcode_flow {
    enum uacpi_opcode_type type : 4;
    bool has_operand : 1;
};

struct uacpi_opcode_create {
    enum uacpi_opcode_type type : 4;
};

struct uacpi_opcode_method_call {
    enum uacpi_opcode_type type : 4;
    struct uacpi_namespace_node *node;
};

struct uacpi_opcode_info {
#ifdef UACPI_EXTENDED_OPCODE_INFO
    const uacpi_char *name;
    uacpi_aml_op code;
#endif

    union {
        enum uacpi_opcode_type type : 4;
        struct uacpi_opcode_arg as_arg;
        struct uacpi_opcode_exec as_exec;
        struct uacpi_opcode_flow as_flow;
        struct uacpi_opcode_create as_create;
        struct uacpi_opcode_method_call as_method_call;
    };
};
extern struct uacpi_opcode_info uacpi_opcode_table[];

#define UACPI_EXT_PREFIX 0x5B
#define UACPI_EXT_OP(op) ((UACPI_EXT_PREFIX << 8) | (op))

#define UACPI_ARG_OPCODE(...) \
    .as_arg = { .type = UACPI_OPCODE_TYPE_ARG, __VA_ARGS__ }

#define UACPI_CREATE_OPCODE(...) \
    .as_create = { .type = UACPI_OPCODE_TYPE_CREATE, __VA_ARGS__ }

#define UACPI_EXEC_OPCODE(...) \
    .as_exec = { .type = UACPI_OPCODE_TYPE_EXEC, __VA_ARGS__ }

#define UACPI_FLOW_OPCODE(...) \
    .as_flow = { .type = UACPI_OPCODE_TYPE_FLOW, __VA_ARGS__ }

#define UACPI_METHOD_CALL_OPCODE(...) \
    .as_method_call = { .type = UACPI_OPCODE_TYPE_METHOD_CALL, __VA_ARGS__ }

#define UACPI_ENUMERATE_OPCODES                     \
    UACPI_OP(                                       \
        ZeroOp, 0x00,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_NUMBER,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        OneOp, 0x01,                                \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_NUMBER,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        BytePrefix, 0x0A,                           \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_NUMBER,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        WordPrefix, 0x0B,                           \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_NUMBER,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        DWordPrefix, 0x0C,                          \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_NUMBER,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        StringPrefix, 0x0D,                         \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_STRING,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        QWordPrefix, 0x0E,                          \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_NUMBER,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        MethodOp, 0x14,                             \
        UACPI_CREATE_OPCODE()                       \
    )                                               \
    UACPI_OP(                                       \
        StoreOp, 0x70,                              \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 2,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        ReturnOp, 0xA4,                             \
        UACPI_FLOW_OPCODE(                          \
            .has_operand = UACPI_TRUE               \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        DebugOp, UACPI_EXT_OP(0x31),                \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_SPECIAL,     \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        UACPIInternalOpMethodCall, 0xFE,            \
        UACPI_METHOD_CALL_OPCODE()                  \
    )

enum uacpi_aml_op {
#define UACPI_OP(name, code, ...) UACPI_AML_OP_##name = code,
    UACPI_ENUMERATE_OPCODES
#undef UACPI_OP
};
