// Name: Modify Local0 Using RefOf Local1
// Expect: int => 0x124

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Local0 = RefOf(Local1)
        Local0 = 0x123
        Debug = Local0
        Debug = Local1
        Return(Local1++)
    }
}
