// Name: Break w/ deep call stack
// Expect: int => 1

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MD4, 0) { Return (0) }
    Method (MD3, 0) { Return (MD4()) }
    Method (MD2, 0) { Return (MD3()) }
    Method (MD1, 0) { Return (MD2()) }

    Method (MWHL, 0) {
        While (One) {
            MD1 ()
            Break
        }
        Return (1)
    }

    Method (MC, 0) { Return (MWHL()) }
    Method (MB, 0) { Return (MC()) }
    Method (MA, 0) { Return (MB()) }

    Method (MAIN, 0, NotSerialized)
    {
        Return (MA())
    }
}
