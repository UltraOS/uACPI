// Name: Increment/Decrement With While
// Expect: int => 10

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 1
        Local1 = 10

        While (Local1--) {
            Debug = "Iteration"
            Debug = Local0

            Local0++
        }

        Return(Local0)
    }
}
