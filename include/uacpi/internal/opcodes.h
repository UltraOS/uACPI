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
    UACPI_ARG_TYPE_ANY = 0,
    UACPI_ARG_TYPE_NUMBER = 1,
    UACPI_ARG_TYPE_STRING = 2,
    UACPI_ARG_TYPE_DEBUG = 3,
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
    uacpi_u32 start_offset, end_offset;
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
        Local0Op, 0x60,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Local1Op, 0x61,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Local2Op, 0x62,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Local3Op, 0x63,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Local4Op, 0x64,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Local5Op, 0x65,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Local6Op, 0x66,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Local7Op, 0x67,                             \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_LOCAL    \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Arg0Op, 0x68,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_ARG      \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Arg1Op, 0x69,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_ARG      \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Arg2Op, 0x6A,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_ARG      \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Arg3Op, 0x6B,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_ARG      \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Arg4Op, 0x6C,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_ARG      \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Arg5Op, 0x6D,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_ARG      \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        Arg6Op, 0x6E,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_ANY,         \
            .sub_type = UACPI_ARG_SUB_TYPE_ARG      \
        )                                           \
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
        RefOfOp, 0x71,                              \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 1,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        AddOp, 0x72,                                \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        SubtractOp, 0x74,                           \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        IncrementOp, 0x75,                          \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 1,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        DecrementOp, 0x76,                          \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 1,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        MultiplyOp, 0x77,                           \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        ShiftLeftOp, 0x79,                          \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        ShiftRightOp, 0x7A,                         \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        AndOp, 0x7B,                                \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        NandOp, 0x7C,                               \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        OrOp, 0x7D,                                 \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        NorOp, 0x7E,                                \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        XorOp, 0x7F,                                \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        DeRefOfOp, 0x83,                            \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 1,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        ModOp, 0x85,                                \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 3,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        LnotOp, 0x92,                               \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_FALSE,              \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 1,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        LEqualOp, 0x93,                             \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_FALSE,              \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 2,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        CopyObjectOp, 0x9D,                         \
        UACPI_EXEC_OPCODE(                          \
            .has_target = UACPI_TRUE,               \
            .has_ret = UACPI_TRUE,                  \
            .operand_count = 2,                     \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        ContinueOp, 0x9F,                           \
        UACPI_FLOW_OPCODE(                          \
            .has_operand = UACPI_FALSE              \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        IfOp, 0xA0,                                 \
        UACPI_FLOW_OPCODE(                          \
            .has_operand = UACPI_TRUE               \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        ElseOp, 0xA1,                               \
        UACPI_FLOW_OPCODE(                          \
            .has_operand = UACPI_FALSE              \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        WhileOp, 0xA2,                              \
        UACPI_FLOW_OPCODE(                          \
            .has_operand = UACPI_TRUE               \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        ReturnOp, 0xA4,                             \
        UACPI_FLOW_OPCODE(                          \
            .has_operand = UACPI_TRUE               \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        BreakOp, 0xA5,                              \
        UACPI_FLOW_OPCODE(                          \
            .has_operand = UACPI_FALSE              \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        DebugOp, UACPI_EXT_OP(0x31),                \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_DEBUG,       \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )                                               \
    UACPI_OP(                                       \
        UACPIInternalOpMethodCall, 0xFE,            \
        UACPI_METHOD_CALL_OPCODE()                  \
    )                                               \
    UACPI_OP(                                       \
        OnesOp, 0xFF,                               \
        UACPI_ARG_OPCODE(                           \
            .arg_type = UACPI_ARG_TYPE_NUMBER,      \
            .sub_type = UACPI_ARG_SUB_TYPE_CONSTANT \
        )                                           \
    )

enum uacpi_aml_op {
#define UACPI_OP(name, code, ...) UACPI_AML_OP_##name = code,
    UACPI_ENUMERATE_OPCODES
#undef UACPI_OP
};
