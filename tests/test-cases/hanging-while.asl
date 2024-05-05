// Name: Infinite While loops eventually ends
// Expect: int => 1

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Name (LOOP, 0)

    Method (HANG) {
        LOOP = 1
        While (LOOP++) { }
    }

    HANG()

    Method (MAIN) {
        Printf("Looped %o times before getting aborted", ToDecimalString(LOOP))

        If (LOOP < 100) {
            Return (0)
        }

        Return (1)
    }
}
