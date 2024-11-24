// Name: IO from deleted fields doesn't crash
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    External(MAIN)

    If (!CondRefOf(MAIN)) {
        Name (MAIN, Ones)

        /*
         * Load ourselves again, we won't enter this branch a second time.
         * We expect this LoadTable call to fail because of the invalid
         * field store.
         */
        MAIN = LoadTable("DSDT", "uTEST", "TESTTABL", "", "", 0)
        Return (0)
    }

    Method (TEST) {
        OperationRegion(MYRE, SystemMemory, 0, 128)
        Field (MYRE, AnyAcc, NoLock) {
            FILD, 32
        }

        FILD = 1
        Return (RefOf(FILD))
    }

    Name (X, "")

    /*
     * Get a dangling field object and make X be this field.
     * Then attempt to perform a read from it.
     */
    Local0 = TEST()
    CopyObject(DerefOf(Local0), X)

    Debug = X
}
