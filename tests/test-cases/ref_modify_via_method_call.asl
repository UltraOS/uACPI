// Name: Modify Local0 Using Arg0
// Expect: int => 124

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 1, NotSerialized)
    {
        Local0 = RefOf(Arg0)
        Local0 = 123
        Arg0++
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 200
        TEST(RefOf(Local0))
        Return (Local0)
    }
}
