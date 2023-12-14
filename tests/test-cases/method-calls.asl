// Name: Call methods with every ArgX
// Expect: int => 8

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method(TES7, 7) {
        Local0 = Arg0 + Arg1 + Arg2 + Arg3 + Arg4 + Arg5 + Arg6
        If (Local0 != (1 + 2 + 3 + 4 + 5 + 6 + 7)) { Return (Local0) }
        Return (1)
    }

    Method(TES6, 6) {
        Local0 = Arg0 + Arg1 + Arg2 + Arg3 + Arg4 + Arg5
        If (Local0 != (1 + 2 + 3 + 4 + 5 + 6)) { Return (Local0) }
        Return (1)
    }

    Method(TES5, 5) {
        Local0 = Arg0 + Arg1 + Arg2 + Arg3 + Arg4
        If (Local0 != (1 + 2 + 3 + 4 + 5)) { Return (Local0) }
        Return (1)
    }

    Method(TES4, 4) {
        Local0 = Arg0 + Arg1 + Arg2 + Arg3
        If (Local0 != (1 + 2 + 3 + 4)) { Return (Local0) }
        Return (1)
    }

    Method(TES3, 3) {
        Local0 = Arg0 + Arg1 + Arg2
        If (Local0 != (1 + 2 + 3)) { Return (Local0) }
        Return (1)
    }

    Method(TES2, 2) {
        Local0 = Arg0 + Arg1
        If (Local0 != (1 + 2)) { Return (Local0) }
        Return (1)
    }

    Method(TES1, 1) {
        Local0 = Arg0
        If (Local0 != (1 + 2)) { Return (Local0) }
        Return (1)
    }

    Method(TES0, 0) { Return (1) }

    Method (MAIN)
    {
        Return(TES7(1, 2, 3, 4, 5, 6, 7) +
               TES6(1, 2, 3, 4, 5, 6) +
               TES5(1, 2, 3, 4, 5) +
               TES4(1, 2, 3, 4) +
               TES3(1, 2, 3) +
               TES2(1, 2) +
               TES1(1) +
               TES0())
    }
}
