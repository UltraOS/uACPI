// Name: Modification via LocalX reference implict-casts (string->int64)
// Expect: int => 0x676E6F6C79726576

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 1, NotSerialized)
    {
        Local0 = RefOf(Arg0)
        Local0 = "verylongstringbiggerthanint"
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 0xDEADC0DEDEADBEEF
        TEST(RefOf(Local0))
        Return (Local0)
    }
}
