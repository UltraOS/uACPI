// Name: Return String Using Local0
// Expect: str => hello world

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Debug = (Local0 = "hello world")
        Return (Local0)
    }
}
