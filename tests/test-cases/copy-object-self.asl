// Name: CopyObject on yourself works
// Expect: str => Hello World

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (BAR, 1, Serialized) {
        Debug = "Enter BAR"
        CopyObject (Arg0, BAR)
        Debug = "Leave BAR"
    }

    Method (FOO) {
        Debug = "Enter FOO"
        CopyObject("Hello", FOO)
        BAR(" World")
        Debug = "Leave FOO"

        Return (0x123)
    }

    Method (MAIN) {
        Local0 = FOO()
        Printf("First invocation of FOO returned %o", Local0)

        Return (Concatenate(FOO, BAR))
    }
}
