// Name: Return Integer (DWord 0xCAFEBABE)
// Expect: int => 0xCAFEBABE

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Return (0xCAFEBABE)
    }
}
