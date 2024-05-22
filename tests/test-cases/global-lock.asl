// Name: Global lock works
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (CHEK, 1, Serialized, 15)
    {
        If (Arg0 != 0) {
            Debug = "Failed to acquire the global lock!"
            Return (1)
        }

        Return (0)
    }

    Method (MAIN, 0, Serialized)
    {
        Local0 = 0

        Debug = "Acquiring the lock manually!"

        Local0 += CHEK(Acquire (_GL, 0xFFFF))
        Local0 += CHEK(Acquire (_GL, 0xFFFF))
        Local0 += CHEK(Acquire (_GL, 0xFFFF))
        Local0 += CHEK(Acquire (_GL, 0xFFFF))

        Debug = "Doing a field write..."

        OperationRegion(NVSM, SystemMemory, 0x100000, 128)
        Field (NVSM, AnyAcc, Lock, WriteAsZeros) {
            FILD, 1,
        }

        FILD = 1

        Debug = "Write done, we should still be holding the lock!"
        Release(_GL)
        Release(_GL)
        Release(_GL)

        Debug = "Should release NOW!"
        Release(_GL)

        // TODO? Would be nice to have some way to actually verify that a lock is held...

        Return (Local0)
    }
}
