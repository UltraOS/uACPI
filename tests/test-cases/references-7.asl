// Name: ArgX non-reference is rebindable
// Expect: str => MyString

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TES0, 1, NotSerialized)
    {
        CopyObject("Hello World", Arg0)
        Debug = Arg0
    }

    Method (TES1, 1, NotSerialized)
    {
        Arg0 = 0x123
        Debug = Arg0
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = "MyString"
        TES0(Local0)
        TES1(Local0)
        Return(Local0)
    }
}
