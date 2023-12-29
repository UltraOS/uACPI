// Name: Out of order mutex release
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
        Mutex(MUT0)
        Mutex(MUT1)
        Mutex(MUT2)
        Mutex(MUT3)
        Mutex(MUT4)
        Mutex(MUT5)
        Mutex(MUT6)
        Mutex(MUT7)
        Mutex(MUT8)
        Mutex(MUT9)
        Mutex(MUTA)
        Mutex(MUTB)
        Mutex(MUTC)
        Mutex(MUTD)
        Mutex(MUTE)
        Mutex(MUTF)

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
        Local0 += CHEK(Acquire(MUT9, 0xF))
        Local0 += CHEK(Acquire(MUTA, 0))
        Local0 += CHEK(Acquire(MUTB, 3))
        Local0 += CHEK(Acquire(MUTC, 123))
        Local0 += CHEK(Acquire(MUTD, 0xFFFE))
        Local0 += CHEK(Acquire(MUTE, 0xFFFD))
        Local0 += CHEK(Acquire(MUTF, 0xFFFF))

        Release(MUTA)
        Release(MUT9)
        Release(MUTC)
        Release(MUT1)
        Release(MUT7)
        Release(MUTE)
        Release(MUTD)

        // The rest are released automatically when we exit the outermost method
        Return (Local0)
    }
}
