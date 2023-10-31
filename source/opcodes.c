#include <uacpi/internal/opcodes.h>

#ifdef UACPI_EXTENDED_OPCODE_INFO
#define UACPI_OP(opname, opcode, ...) \
    { .name = #opname, .code = opcode, __VA_ARGS__ },
#else
#define UACPI_OP(opname, opcode, ...) \
    { __VA_ARGS__ },
#endif

struct uacpi_opcode_info uacpi_opcode_table[] = {
    UACPI_ENUMERATE_OPCODES
    { 0 }
};
