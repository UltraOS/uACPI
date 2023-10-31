// Name: Return Integer 0xDEAD (Double Indirect)
// Expect: int => 0xDEAD

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (HELP, 0, NotSerialized)
    {
        Return (0xDEAD)
    }

    Method (INDI, 0, NotSerialized)
    {
        Return (HELP())
    }

    Method (MAIN, 0, NotSerialized)
    {
        Return (INDI())
    }
}
