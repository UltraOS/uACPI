// Name: Infinite recursion eventually ends
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (MAIN, 0xDEADBEEF)
    Name (ITER, 0)

    Method (HANG) {
        ITER++
        HANG()
    }

    HANG()
    Printf("Recursed %o times before stopping", ToDecimalString(ITER))

    If (ITER > 64) {
        MAIN = 0
    } Else {
        Debug = "Recursion depth was too small"
    }
}
