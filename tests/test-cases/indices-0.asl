// Name: Buffer Indices
// Expect: str => HfVXoWorld

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (FAIL, 2)
    {
        Printf("Invalid string %o, expected %o", Arg0, Arg1)
        Return(1)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = "HelloWorld"
        Local0[3] = "X"

        Local1 = "HelXoWorld"
        If (Local0 != Local1) {
            Return(FAIL(Local0, Local1))
        }

        Local2 = RefOf(Index(Local0, 2))
        Local2 = "V"

        Local1 = "HeVXoWorld"
        If (Local0 != Local1) {
            Return(FAIL(Local0, Local1))
        }

        CopyObject(Index(Local0, 1), Local2)
        Local0[1] = 0x66

        If (DerefOf(Local2) != 0x66) {
            Return(1)
        }

        Return (Local0)
    }
}
