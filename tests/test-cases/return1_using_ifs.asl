// Name: Return Integer 1 Using Ifs
// Expect: int => 1

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (GET0, 1, NotSerialized)
    {
        Debug = "GET0 called"
        Return(Arg0)
    }

    Method (MAIN, 0, NotSerialized)
    {
        If (GET0(0)) {
            Debug = "We shouldn't be here..."
        } Else {
            Local0 = 0

            If (GET0(1)) {
                Debug = "Branch worked"
                Local0 = 1
            } Else {
                Debug = "Shouldn't see this either"
                Local0 = 2
            }

            Return(Local0)
        }

        Return (3)
    }
}
