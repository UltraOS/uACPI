// Name: Call-by-value w/ Store() modifies strings through a reference
// Expect: str => WHY?
// NOTE:
// This test seems bogus but it's actually corrrect, it produces
// the same output on NT.

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (TEST, 1, NotSerialized)
    {
        Local0 = RefOf(Arg0)

        // WHY? in little-endian ASCII
        Local0 = 0x3F594857
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local0 = "MyST"
        TEST(Local0)
        Return (Local0)
    }
}
