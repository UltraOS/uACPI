// Name: Store Copies The Buffer
// Expect: str => Hello

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MODF, 0, NotSerialized)
    {
        Local1 = Store("Hello", Local0)
        Return (Local1)
    }

    Method (MAIN, 0, NotSerialized)
    {
        Local1 = Store(MODF(), Local0)

        // Modify the string at Local1
        Local2 = RefOf(Local1)
        Local2 = "Goodbye"

        Debug = Local0
        Debug = Local1

        // Local0 should still have the same value
        Return(Local0)
    }
}
