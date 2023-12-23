#include <uacpi/internal/opcodes.h>

#define UACPI_OP(opname, opcode, ...) \
    { #opname, .decode_ops = __VA_ARGS__, .code = opcode },

#define UACPI_OUT_OF_LINE_OP(opname, opcode, out_of_line_buf, props) \
    {                                                                \
      .name = #opname,                                               \
      .indirect_decode_ops = out_of_line_buf,                        \
      .properties = props,                                           \
      .code = opcode,                                                \
    },

static const struct uacpi_op_spec opcode_table[0x100] = {
    UACPI_ENUMERATE_OPCODES
};

static const struct uacpi_op_spec ext_opcode_table[] = {
    UACPI_ENUMERATE_EXT_OPCODES
};

#define _(op) (op & 0x00FF)

const static uacpi_u8 ext_op_to_idx[0x100] = {
    [_(UACPI_AML_OP_MutexOp)]       = 1,  [_(UACPI_AML_OP_EventOp)]       = 2,
    [_(UACPI_AML_OP_CondRefOfOp)]   = 3,  [_(UACPI_AML_OP_CreateFieldOp)] = 4,
    [_(UACPI_AML_OP_LoadTableOp)]   = 5,  [_(UACPI_AML_OP_LoadOp)]        = 6,
    [_(UACPI_AML_OP_StallOp)]       = 7,  [_(UACPI_AML_OP_SleepOp)]       = 8,
    [_(UACPI_AML_OP_AcquireOp)]     = 9,  [_(UACPI_AML_OP_SignalOp)]      = 10,
    [_(UACPI_AML_OP_WaitOp)]        = 11, [_(UACPI_AML_OP_ResetOp)]       = 12,
    [_(UACPI_AML_OP_ReleaseOp)]     = 13, [_(UACPI_AML_OP_FromBCDOp)]     = 14,
    [_(UACPI_AML_OP_ToBCDOp)]       = 15, [_(UACPI_AML_OP_RevisionOp)]    = 16,
    [_(UACPI_AML_OP_DebugOp)]       = 17, [_(UACPI_AML_OP_FatalOp)]       = 18,
    [_(UACPI_AML_OP_TimerOp)]       = 19, [_(UACPI_AML_OP_OpRegionOp)]    = 20,
    [_(UACPI_AML_OP_FieldOp)]       = 21, [_(UACPI_AML_OP_DeviceOp)]      = 22,
    [_(UACPI_AML_OP_ProcessorOp)]   = 23, [_(UACPI_AML_OP_PowerResOp)]    = 24,
    [_(UACPI_AML_OP_ThermalZoneOp)] = 25, [_(UACPI_AML_OP_IndexFieldOp)]  = 26,
    [_(UACPI_AML_OP_BankFieldOp)]   = 27, [_(UACPI_AML_OP_DataRegionOp)]  = 28,
};

const struct uacpi_op_spec *uacpi_get_op_spec(uacpi_aml_op op)
{
    if (op > 0xFF)
        return &ext_opcode_table[ext_op_to_idx[_(op)]];

    return &opcode_table[op];
}

#define PARSE_FIELD_ELEMENTS(parse_loop_pc)                            \
    /* Parse every field element found inside */                       \
    UACPI_PARSE_OP_IF_HAS_DATA, 44,                                    \
        /* Look at the first byte */                                   \
        UACPI_PARSE_OP_LOAD_IMM, 1,                                    \
                                                                       \
        /* ReservedField := 0x00 PkgLength */                          \
        UACPI_PARSE_OP_IF_EQUALS, 0x00, 3,                             \
            UACPI_PARSE_OP_PKGLEN,                                     \
            UACPI_PARSE_OP_JMP, parse_loop_pc,                         \
                                                                       \
        /* AccessField := 0x01 AccessType AccessAttrib */              \
        UACPI_PARSE_OP_IF_EQUALS, 0x01, 6,                             \
            UACPI_PARSE_OP_LOAD_IMM, 1,                                \
            UACPI_PARSE_OP_LOAD_IMM, 1,                                \
            UACPI_PARSE_OP_JMP, parse_loop_pc,                         \
                                                                       \
        /* ConnectField := <0x02 NameString> | <0x02 BufferData> */    \
        UACPI_PARSE_OP_IF_EQUALS, 0x02, 5,                             \
            UACPI_PARSE_OP_TERM_ARG_UNWRAP_INTERNAL,                   \
            UACPI_PARSE_OP_TYPECHECK, UACPI_OBJECT_BUFFER,             \
            UACPI_PARSE_OP_JMP, parse_loop_pc,                         \
                                                                       \
        /* ExtendedAccessField := 0x03 AccessType ExtendedAccessAttrib \
         *                                        AccessLength */      \
        UACPI_PARSE_OP_IF_EQUALS, 0x03, 8,                             \
            UACPI_PARSE_OP_LOAD_IMM, 1,                                \
            UACPI_PARSE_OP_LOAD_IMM, 1,                                \
            UACPI_PARSE_OP_LOAD_IMM, 1,                                \
            UACPI_PARSE_OP_JMP, parse_loop_pc,                         \
                                                                       \
        /* NamedField := NameSeg PkgLength */                          \
                                                                       \
        /*                                                             \
         * Discard the immediate, as it's the first byte of the        \
         * nameseg. We don't need it.                                  \
         */                                                            \
        UACPI_PARSE_OP_ITEM_POP,                                       \
        UACPI_PARSE_OP_AML_PC_DECREMENT,                               \
        UACPI_PARSE_OP_CREATE_NAMESTRING,                              \
        UACPI_PARSE_OP_PKGLEN,                                         \
        UACPI_PARSE_OP_OBJECT_ALLOC_TYPED, UACPI_OBJECT_UNIT_FIELD,    \
        UACPI_PARSE_OP_JMP, parse_loop_pc,                             \
                                                                       \
    UACPI_PARSE_OP_INVOKE_HANDLER,                                     \
    UACPI_PARSE_OP_END

uacpi_u8 uacpi_field_op_decode_ops[] = {
    UACPI_PARSE_OP_TRACKED_PKGLEN,
    UACPI_PARSE_OP_EXISTING_NAMESTRING,
    UACPI_PARSE_OP_LOAD_IMM, 1,
    PARSE_FIELD_ELEMENTS(4),
};

uacpi_u8 uacpi_bank_field_op_decode_ops[] = {
    UACPI_PARSE_OP_TRACKED_PKGLEN,
    UACPI_PARSE_OP_EXISTING_NAMESTRING,
    UACPI_PARSE_OP_EXISTING_NAMESTRING,
    UACPI_PARSE_OP_OPERAND,
    UACPI_PARSE_OP_LOAD_IMM, 1,
    PARSE_FIELD_ELEMENTS(6),
};

uacpi_u8 uacpi_index_field_op_decode_ops[] = {
    UACPI_PARSE_OP_TRACKED_PKGLEN,
    UACPI_PARSE_OP_EXISTING_NAMESTRING,
    UACPI_PARSE_OP_EXISTING_NAMESTRING,
    UACPI_PARSE_OP_LOAD_IMM, 1,
    PARSE_FIELD_ELEMENTS(5),
};
