// Name: Increment And Decrement Fields & Indices
// Expect: int => 150

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (BROK, 2) {
        Printf ("%o increment is broken", Arg0)
        Return (Arg1)
    }

    Method (MAIN, 0, Serialized)
    {
        Local0 = Buffer { 1 }
        Local1 = Package {
            123, 22
        }

        Debug = Increment(Local0[0])
        Debug = Decrement(Local1[0])

        Local2 = Local1[1]
        Debug = Increment(Local2)

        OperationRegion(NVSM, SystemMemory, 0x100000, 128)
        Field (NVSM, AnyAcc, NoLock, WriteAsZeros) {
            FILD, 1,
        }
        FILD = 0

        Debug = Increment(FILD)

        // We get a 2 here but write back 0 because field is one bit
        Local3 = Increment(FILD)
        If (FILD != 0) {
            Return (BROK("Field unit", 0xDEADBEEF))
        }

        Local4 = Increment(FILD)
        If (FILD != 1) {
            Return (BROK("Field unit", 0xBEEFDEAD))
        }

        If (DerefOf(Local2) != DerefOf(Local1[1])) {
            Return (BROK("Buffer index", 0xCAFEBABE))
        }

        Return (
            // 2
            DerefOf(Local0[0]) +
            // 122
            DerefOf(Local1[0]) +
            // 23
            DerefOf(Local2)    +
            // 2
            Local3             +
            // 1
            Local4
        )
    }
}
