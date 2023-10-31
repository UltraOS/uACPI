// Name: Return Integer (QWord 0xCAFEBABEDEADC0DE)
// Expect: int => 0xCAFEBABEDEADC0DE

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Return (0xCAFEBABEDEADC0DE)
    }
}
