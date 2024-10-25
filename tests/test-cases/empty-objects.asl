// Name: Empty objects behave correctly
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (EMPT) { }

    Method (MAIN) {
        Local0 = EMPT()
        Debug = Local0

        Local0 = Package (0) { }
        Debug = Local0

        Local0 = 0
        Local1 = Package(Local0) { }
        Debug = Local1

        Return (0)
    }
}
