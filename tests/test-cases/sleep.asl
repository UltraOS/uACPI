// Name: Sleep & Stall
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (CHEK, 4)
    {
        Local0 = Arg2 - Arg1

        If (Local0 < Arg3) {
            Printf("%o finished too soon, elapsed %o, expected at least %o",
                   Arg0, ToDecimalString(Local0), ToDecimalString(Arg3))
            Return (1)
        }

        Return (0)
    }

    Method (STAL, 1) {
        Local1 = 0

        While (Local1 < Arg0) {
            Stall(100)
            Local1 += 1
        }
    }

    Method (MAIN, 0, Serialized)
    {
        Local0 = Timer

        // Stall for 10 * 100 microseconds (aka 1ms)
        STAL(10)

        If (CHEK("Stall", Local0, Timer, 10000) != 0) {
            Return (1)
        }

        Local0 = Timer

        // Sleep for 2ms
        Sleep(2)

        If (CHEK("Sleep", Local0, Timer, 20000) != 0) {
            Return (1)
        }

        Return (0)
    }
}
