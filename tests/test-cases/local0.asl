// Name: Return Integer Using Local0
// Expect: int => 0x123

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 0x123
        Debug = Local0
        Return (Local0)
    }
}
