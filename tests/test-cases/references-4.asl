// Name: Call-by-value w/ Store() does NOT modify integers even through a reference
// Expect: int => 0x123

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 1, NotSerialized)
    {
        Local0 = RefOf(Arg0)
        Local0 = 0xDEADBEEF
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 0x123
        TEST(Local0)
        Return (Local0)
    }
}
