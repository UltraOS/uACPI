#include <uacpi/internal/opcodes.h>

#define UACPI_OP(opname, opcode, ...) \
    { #opname, opcode, __VA_ARGS__ },

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
    [_(UACPI_AML_OP_ReleaseOp)]     = 13, [_(UACPI_AML_OP_FromBDCOp)]     = 14,
    [_(UACPI_AML_OP_ToBCD)]         = 15, [_(UACPI_AML_OP_RevisionOp)]    = 16,
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
