// Name: Unresolved paths don't error out when needed
// Expect: int => 1

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Local0 = Package {
            321,
            PATH.THAT.DOES.NOT_.EXIS.T,
            123,
            \OK,
            \MAIN,
        }
        Debug = Local0

        If (CondRefOf(ANOT.HER_.PATH.THAT.DOES.NOT.EXIS.T, Local0) ||
            CondRefOf(\TEST) || CondRefOf(^TEST) || CondRefOf(^TEST.JEST) ||
            CondRefOf(\TEST.JEST)) {
            Return (0)
        }

        If (CondRefOf(^MAIN, Local1)) {
            Debug = Local1
            Return (1)
        }

        Return (0)
    }
}
