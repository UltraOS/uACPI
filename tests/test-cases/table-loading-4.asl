// Name: LoadTable scoping rules are correct
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    External(SSDT, IntObj)
    External(VAL, IntObj)

    // All dynamic loads branch into here
    If (CondRefOf(SSDT)) {
        Name (TEST, 0)
        TEST = VAL
        Return (Package { 1 })
    }

    Name (SSDT, 123)
    Name (VAL, 1)

    Device (DEV0) {
        Name (LRES, 0)
    }

    Device (DEV1) { }
    Scope (_SB) {
        Device (DEV1) {
            Name (LRES, "XX")
            Device (DEV1) { }
        }
    }

    Name (LRES, 99)
    Device (DEV2) {
        Name (LRES, 123)
    }

    Method (LDTB, 3) {
        Local0 = LoadTable("DSDT", "uTEST", "TESTTABL", Arg0, Arg1, Arg2)
        VAL += 1

        If (!Local0) {
            Printf("Table load failed!")
            Return (0)
        }

        Return (1)
    }

    Method (MAIN)
    {
        /*
         * DEV0 is the scope, LRES should be evaluated relative to it.
         * TEST is expected to be loaded there as well.
         */
        If (!LDTB("DEV0", "LRES", 0xFEBEFEBE)) {
            Return (1)
        }
        If (!CondRefOf(\DEV0.TEST, Local0)) {
            Printf("No TEST under \\DEV0")
            Return (1)
        }
        If (DerefOf(Local0) != 1) {
            Printf("Incorrect \\DEV0.TEST value %o", DerefOf(Local0))
            Return (1)
        }
        If (\DEV0.LRES != 0xFEBEFEBE) {
            Printf("\\DEV0.LRES has an incorrect value %o", \DEV0.LRES)
            Return (1)
        }

        CopyObject(0, Local0)

        Scope (\_SB.DEV1) {
            /*
             * We're already inside \_SB.DEV1, so this DEV1 should match
             * \_SB.DEV1.DEV1, note that there's also \DEV1, that shouldn't
             * get matched here.
             *
             * There's, however, no \_SB.DEV1.DEV1.LRES, so this should resolve
             * into \_SB.DEV1.LRES instead.
             */
            Local0 = LoadTable("DSDT", "uTEST", "TESTTABL", "DEV1", "LRES", 0x4B4F)
            If (!Local0) {
                Printf("Table load failed!")
                Return (0)
            }
            VAL += 1
        }
        If (!CondRefOf(\_SB.DEV1.DEV1.TEST, Local0)) {
            Printf("No TEST under _SB.DEV1.DEV1")
            Return (1)
        }
        If (DerefOf(Local0) != 2) {
            Printf("Incorrect \\_SB.DEV1.DEV1.TEST value %o", DerefOf(Local0))
        }
        If (\_SB.DEV1.LRES != "OK") {
            Printf("DEV1.LRES has an incorrect value %o", \_SB.DEV1.LRES)
            Return (1)
        }

        CopyObject(0, Local0)

        /*
         * DEV2 relative load, however, LRES is specified as an absolute path
         * so it shouldn't get resolved to DEV2.LRES.
         */
        If (!LDTB("DEV2", "\\LRES", 0xCAFEBABE)) {
            Return (1)
        }
        If (!CondRefOf(\DEV2.TEST, Local0)) {
            Printf("No TEST under \\DEV2")
            Return (1)
        }
        If (DerefOf(Local0) != 3) {
            Printf("Incorrect \\DEV2.TEST value %o", DerefOf(Local0))
            Return (1)
        }
        If (\LRES != 0xCAFEBABE) {
            Printf("\\LRES has an incorrect value %o", \LRES)
            Return (1)
        }

        Return (0)
    }
}
