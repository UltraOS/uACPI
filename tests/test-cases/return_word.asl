// Name: Return Integer (Word 0xCAFE)
// Expect: int => 0xCAFE

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Return (0xCAFE)
    }
}
