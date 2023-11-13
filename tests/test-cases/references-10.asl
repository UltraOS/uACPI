// Name: Recursively modify ArgX
// Expect: int => 0x100A

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 2, NotSerialized)
    {
        Local0 = RefOf(Arg0)
        Local0++

        Debug = Arg1
        Debug = DerefOf(Local0)
        If (Arg1--) {
            TEST(RefOf(Arg0), Arg1)
        }
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 0x1000
        TEST(RefOf(Local0), 10)
        Return (Local0)
    }
}
