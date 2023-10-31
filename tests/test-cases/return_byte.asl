// Name: Return Integer (Byte 0xCA)
// Expect: int => 0xCA

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Return (0xCA)
    }
}
