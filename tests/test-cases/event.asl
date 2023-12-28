// Name: Event signal & wait
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, Serialized)
    {
        Event (EVET)

        Local0 = 5
        While (Local0--) {
            Signal(EVET)
        }

        Local0 = 5
        While (Local0--) {
            Local1 = Wait(EVET, 0xFFFD + Local0)
            If (Local1 != Zero) {
                Return (Local1)
            }
        }

        // This should fail
        Local1 = Wait(EVET, Zero)
        If (Local1 == Zero) {
            Return (One)
        }

        Return (Zero)
    }
}
