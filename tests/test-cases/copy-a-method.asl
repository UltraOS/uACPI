// Name: Copy a local method and execute it
// Expect: int => 3735928559

DefinitionBlock ("x.aml", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (TEST, "Hello world!")

    Method (GETX) {
        Return (1)
    }

    Method (COPY) {
        Method (GETX, 1, Serialized) {
            Name (Y, 0xDEAD0000)
            Y += Arg0
            Return (Y)
        }

        Return (RefOf(GETX))
    }

    Method (COP1) {
        Local0 = COPY()
        Return (DerefOf(Local0))
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = COP1()
        CopyObject(Local0, TEST)

        Return (TEST(0xBEEF))
    }
}
