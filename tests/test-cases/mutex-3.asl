// Name: Recursive mutex
// Expect: int => 253

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (CHEK, 1, Serialized, 15)
    {
        If (Arg0 != 0) {
            Debug = "Failed to acquire mutex!"
            Return (1)
        }

        Return (0)
    }

    Mutex (MUTX)

    Method (ACQ, 1, Serialized) {
        CHEK(Acquire(MUTX, 0xFFFF))

        Local0 = 0

        If (Arg0 < 22) {
            Local0 += Arg0 + ACQ(Arg0 + 1)
        } Else {
            Local0 += Arg0
        }

        Release(MUTX)
        Return (Local0)
    }

    Method (MAIN, 0, Serialized)
    {
        Return (ACQ(0))
    }
}
