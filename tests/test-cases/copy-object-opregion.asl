// Name: CopyObject with Operation Region works
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    Method (REPL, 1) {
        Arg0 = 123
    }

    Method (CAS0) {
        OperationRegion(MYRE, SystemMemory, 0, 128)
        Field (MYRE, AnyAcc, NoLock) {
            FILD, 32
        }
        FILD = 1

        CopyObject(123, MYRE)
    }
    CAS0()

    Method (CAS1) {
        OperationRegion(MYRE, SystemMemory, 0, 128)
        Field (MYRE, AnyAcc, NoLock) {
            FILD, 32
        }
        FILD = 1

        REPL(RefOf(MYRE))
    }
    CAS1()

    Method (CAS2) {
        OperationRegion(MYRE, SystemMemory, 0, 128)
        Field (MYRE, AnyAcc, NoLock) {
            FILD, 32
        }
        FILD = 1

        Name (FAKE, 123)
        CopyObject(MYRE, FAKE)
        Field (FAKE, AnyAcc, NoLock) {
            FAKF, 32
        }

        REPL(RefOf(MYRE))
        FAKF = 1
    }
    CAS2()

    Method (CAS3) {
        OperationRegion(MYR0, SystemMemory, 0, 128)
        OperationRegion(MYR1, SystemMemory, 0, 128)
        CopyObject(123, MYR1)
    }
    CAS3()

    Method (CAS4) {
        OperationRegion(MYRE, SystemMemory, 0, 128)
        Field (MYRE, AnyAcc, NoLock) {
            FILD, 32
        }

        FILD = 1
        CopyObject(FILD, MYRE)
        FILD = 1
    }
    CAS4()

    Name (MAIN, 0)
}
