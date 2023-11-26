// Name: LocalX reference is rebindable via CopyObject
// Expect: str => Modified

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 1, NotSerialized)
    {
        Local0 = RefOf(Arg0)
        Local0 = "Modified String"

        CopyObject("Wrong String", Local0)
        Local0 = 0xDEADBEEF
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = "MyString"
        TEST(Local0)
        Return (Local0)
    }
}
