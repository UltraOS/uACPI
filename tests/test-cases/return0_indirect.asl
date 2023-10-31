// Name: Return Integer 0 (Indirect)
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (HELP, 0, NotSerialized)
    {
        Return (0)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Return (HELP())
    }
}
