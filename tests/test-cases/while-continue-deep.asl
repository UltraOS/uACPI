// Name: Continue w/ deep call stack
// Expect: int => 3

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MD4, 0) { Return (0) }
    Method (MD3, 0) { Return (MD4()) }
    Method (MD2, 0) { Return (MD3()) }
    Method (MD1, 0) { Return (MD2()) }

    Method (MWHL, 0) {
        Local0 = 0

        While (One) {
            Local0++
            If (Local0 < 3) {
                MD1()
                Continue
            }
            Break
        }

        Return (Local0)
    }

    Method (MC, 0) { Return (MWHL()) }
    Method (MB, 0) { Return (MC()) }
    Method (MA, 0) { Return (MB()) }

    Method (MAIN, 0, NotSerialized)
    {
        Return (MA())
    }
}
