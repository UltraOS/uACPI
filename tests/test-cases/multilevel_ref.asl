// Name: Multilevel reference w/ Increment
// Expect: int => 0xDEAD0005

DefinitionBlock ("", "DSDT", 1, "uTTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (MAIN, 0, NotSerialized)
    {
        Local0 = 0xDEAD0000

        Local1 = RefOf(Local0)
        Debug = Local1

        Local2 = RefOf(Local1)
        Debug = Local2

        Local3 = RefOf(Local2)
        Debug = Local3

        Local4 = RefOf(Local3)
        Debug = Local4

        Local5 = RefOf(Local4)
        Debug = Local5

        Debug = Increment(Local1)
        Debug = Increment(Local2)
        Debug = Increment(Local3)
        Debug = Increment(Local4)
        Debug = Increment(Local5)

        Debug = Local0
        Return (DerefOf(Local5))
    }
}
