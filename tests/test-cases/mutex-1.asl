// Name: Automatic mutex release
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (CHEK, 1, Serialized, 15)
    {
        If (Arg0 != 0) {
            Debug = "Failed to acquire mutex!"
            Return (1)
        }

        Return (0)
    }

    Method (MAIN, 0, Serialized)
    {
        Mutex(MUT0, 0)
        Mutex(MUT1, 1)
        Mutex(MUT2, 2)
        Mutex(MUT3, 3)
        Mutex(MUT4, 4)
        Mutex(MUT5, 5)
        Mutex(MUT6, 6)
        Mutex(MUT7, 7)
        Mutex(MUT8, 8)
        Mutex(MUT9, 9)
        Mutex(MUTA, 10)
        Mutex(MUTB, 11)
        Mutex(MUTC, 12)
        Mutex(MUTD, 13)
        Mutex(MUTE, 14)
        Mutex(MUTF, 15)

        Local0 = 0
        Local0 += CHEK(Acquire(MUT0, 0))
        Local0 += CHEK(Acquire(MUT1, 0))
        Local0 += CHEK(Acquire(MUT2, 0))
        Local0 += CHEK(Acquire(MUT3, 0))
        Local0 += CHEK(Acquire(MUT4, 0))
        Local0 += CHEK(Acquire(MUT5, 0))
        Local0 += CHEK(Acquire(MUT6, 0))
        Local0 += CHEK(Acquire(MUT7, 0))
        Local0 += CHEK(Acquire(MUT8, 0))
        Local0 += CHEK(Acquire(MUT9, 0))
        Local0 += CHEK(Acquire(MUTA, 0))
        Local0 += CHEK(Acquire(MUTB, 0))
        Local0 += CHEK(Acquire(MUTC, 0))
        Local0 += CHEK(Acquire(MUTD, 0xFFFE))
        Local0 += CHEK(Acquire(MUTE, 0xFFFD))
        Local0 += CHEK(Acquire(MUTF, 0xFFFF))

        Return (Local0)
    }
}
