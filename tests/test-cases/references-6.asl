// Name: ArgX reference is not rebindable via CopyObject
// Expect: int => 0xDEADBEEF

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 1, NotSerialized)
    {
        CopyObject(0xDEADC0DE, Arg0)
        CopyObject(0xDEADBEEF, Arg0)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = "MyString"
        TEST(RefOf(Local0))
        Return (Local0)
    }
}
