// Name: While With Break
// Expect: int => 10

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 1
        Local1 = 10

        While (Local0) {
            If (Local1--) {
                Debug = "Incrementing Local0"
                Local0++
                Debug = Local0
            } Else {
                Debug = "Local1 is 0, breaking"
                Break
            }
        }

        Return(Local0)
     }
}
