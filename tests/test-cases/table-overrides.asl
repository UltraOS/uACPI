// Name: Table overrides work
// Expect: int => 0

DefinitionBlock ("", "DSDT", 2, "uTEST", "TESTTABL", 0xF0F0F0F0)
{
    /*
     * We expect this table to be denied by the test-runner because it denies
     * anything with "DENYTABL" in OEM table id.
     *
     * DefinitionBlock ("x.aml", "SSDT", 1, "uTEST", "DENYTABL", 0xF0F0F0F0)
     * {
     *     Name (BUG, 1)
     * }
     */
    Name (TAB0, Buffer {
        0x53, 0x53, 0x44, 0x54, 0x2a, 0x00, 0x00, 0x00,
        0x01, 0xe1, 0x75, 0x54, 0x45, 0x53, 0x54, 0x00,
        0x44, 0x45, 0x4e, 0x59, 0x54, 0x41, 0x42, 0x4c,
        0xf0, 0xf0, 0xf0, 0xf0, 0x49, 0x4e, 0x54, 0x4c,
        0x28, 0x06, 0x23, 0x20, 0x08, 0x42, 0x55, 0x47,
        0x5f, 0x01
    })

    /*
     * We expect this table to be overriden by the test-runner because it
     * overrides anything with "OVERTABL" in OEM table id. The override it
     * provides has a Name(VAL, "TestRunner")
     *
     * DefinitionBlock ("x.aml", "SSDT", 1, "uTEST", "OVERTABL", 0xF0F0F0F0)
     * {
     *     Name (VAL, "Hello")
     * }
     */
    Name (TAB1, Buffer {
        0x53, 0x53, 0x44, 0x54, 0x30, 0x00, 0x00, 0x00,
        0x01, 0xcd, 0x75, 0x54, 0x45, 0x53, 0x54, 0x00,
        0x4f, 0x56, 0x45, 0x52, 0x54, 0x41, 0x42, 0x4c,
        0xf0, 0xf0, 0xf0, 0xf0, 0x49, 0x4e, 0x54, 0x4c,
        0x25, 0x09, 0x20, 0x20, 0x08, 0x56, 0x41, 0x4c,
        0x5f, 0x0d, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x00
    })

    Method (MAIN, 0, NotSerialized)
    {
        Load(TAB0, Local0)
        If (Local0) {
            Debug = "Table was not denied load"
            Return (0xDEAD)
        }

        If (CondRefOf(BUG)) {
            Debug = "Table was not loaded but the BUG object exists"
            Return (0xDEAD)
        }

        Load(TAB1, Local0)
        If (!Local0) {
            Debug = "Failed to load table"
            Return (0xDEAD)
        }

        If (VAL != "TestRunner") {
            Printf("Table didn't get overriden correctly: expected %o got %o",
                   "TestRunner", VAL)
            Return (0xDEAD)
        }

        Return (0)
    }
}
